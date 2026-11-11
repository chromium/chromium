# Security Guidance for //remoting (Chrome Remote Desktop)

This document defines the security architecture, process trust boundaries,
threat assumptions, and component-specific non-bugs for Chrome Remote Desktop
(CRD / Chromoting).

Before filing a security bug against `//remoting`—especially if using automated
or AI-assisted vulnerability discovery—review
[Security for Agents](../docs/security/security-for-agents.md) and verify that
your finding crosses an actual trust boundary documented below.

## Scope

This policy applies to production code in `//remoting/` and its subdirectories.

It does **not** apply to:
* Test-only targets, mocks, fakes, or manual developer tools
  (`//remoting/test/`, `*_unittest.cc`, `fake_*`, `mock_*`,
  `remoting_dev_me2me_host`).
* The Chrome Remote Desktop web client UI, which lives outside the Chromium
  repository.

On desktop platforms (Linux, Windows, macOS), CRD Me2Me (Remote Access) runs as
a standalone OS service/daemon independent of the Chrome browser process, and
Chrome browser renderer processes only interact with `//remoting` through
Native Messaging hosts (`remoting_native_messaging_host` and
`remote_assistance_host`) restricted by manifest allowlists to Chrome Remote
Desktop origins. **Exception — ChromeOS:** On ChromeOS, both the host
(`//remoting/host/chromeos/`) and native Boca client (`//remoting/client/`) run
in-process inside the unsandboxed Ash `chrome` browser binary (see below).

## Security Boundaries & Threat Model

### Multi-Process Me2Me Architecture

<!-- TODO(crbug.com/499082867): Add macOS details once multi-process lands. -->

Desktop Me2Me hosts use a **4-process** architecture (`daemon`, `network`,
`peer_connection`, and `desktop`) plus short-lived user-session helper
binaries:

| Process | Binary / `--type` | OS Identity & Sandbox | Trust Tier | Responsibilities |
| :--- | :--- | :--- | :--- | :--- |
| **Daemon** | `remoting_me2me_host --type=daemon` (Linux)<br/>`remoting_host.exe` (Windows) | **Linux:** `root` (unsandboxed)<br/>**Windows:** `SYSTEM`, `SYSTEM` MIC, Session 0 (unsandboxed) | **Trusted (TCB)** | Mojo IPC broker; reads root/SYSTEM-owned configs; spawns worker processes; manages OS display/session lifecycle (GDM / `systemd-logind` on Linux, WTS / loopback RDP on Windows); brokers `CertificateBroker` (Windows) and `ChromotingHostServices` sockets. |
| **Network** | `remoting_me2me_host --type=network` (Linux)<br/>`remoting_network.exe` (Windows) | **Linux:** `_crd_network` service user<br/>**Windows:** `LOCAL SERVICE`, Low MIC, Session 0 | **Trusted Control Plane (TCB)** | Holds host RSA private key, service-account OAuth refresh token, PIN hash, and client pairing secrets; watches enterprise policies (`PolicyWatcher`); communicates over HTTPS/TLS with Google directory/signaling/authz backends (`FtlSignalStrategy`, `CorpSignalStrategy`, `SessionAuthz`); performs `ClientSession` authentication (`NegotiatingHostAuthenticator`). |
| **PeerConnection** | `remoting_me2me_host --type=peer_connection` (Linux)<br/>`remoting_peer_connection.exe` (Windows) | **Linux:** `_crd_peer_connection` user + Landlock + multi-threaded Seccomp-BPF + `PR_SET_DUMPABLE=0`<br/>**Windows:** `LOCAL SERVICE` inside AppContainer (`chromoting.peer_connection`, Untrusted MIC) | **Untrusted Data Plane** | Spawned per session **only after** client authentication succeeds in `network`. Defends against a compromised or malicious client endpoint or an attacker on the P2P/TURN path by isolating WebRTC transport (ICE/DTLS/SRTP/SCTP), audio/video codecs, protobuf data channels, and `ChromotingSessionServices` (`WebAuthnProxy`, `RemoteUrlOpener`, `SecurityKeyForwarder`). |
| **Desktop** | `remoting_me2me_host --type=desktop` (Linux)<br/>`remoting_desktop.exe` (Windows) | **Linux:** Target session user (unsandboxed, attached to user's `systemd` session cgroup)<br/>**Windows:** `SYSTEM`, `SYSTEM` MIC, `uiAccess="true"` in user WTS session | **Linux:** Untrusted relative to `daemon`, `network`, and other local users<br/>**Windows:** Trusted (`SYSTEM`) | Captures screen/audio, injects input, and executes file-transfer operations (`SessionFileOperationsHandler`). On Linux, also listens on `/run/user/<uid>/crd_ssh_auth_sock` for security-key forwarding. On Windows, spawns a low-privilege user-token `file_chooser` child process for file picker UI. |
| **User Helpers** | **Linux:** `remote-open-url`, `remote-webauthn`, `remote-session-info`<br/>**Windows:** `remote_open_url.exe`, `remote_webauthn.exe`, `remote_security_key.exe` | Logged-in session user | **Untrusted** | Connect to `ChromotingHostServices` (or `LoginSession` for `remote-session-info`) over a named socket/pipe; `daemon` verifies caller UID / WTS session ID and executable path before binding session services. |

#### Rollout Status

<!-- TODO(crbug.com/502281489, crbug.com/499082867): Remove once 4-process
     Me2Me rollout completes on Linux, Windows, and macOS. -->

* **Linux:** The 4-process multi-process host is feature-complete but not yet
  enabled by default; most Linux Me2Me hosts currently run in legacy
  single-process mode (`single_process_host`, running `remoting_me2me_host` as
  the target local user inside a dedicated Xvfb/Xorg virtual desktop session).
* **Windows (`crbug.com/502281489`):** Corp Windows hosts use the 4-process
  model by default. Non-Corp Windows hosts currently use the legacy 3-process
  layout (where `network` also hosts WebRTC and `DesktopSession` proxies as
  `LOCAL SERVICE` at Low Integrity) until 4-process rollout completes.
* **macOS (`crbug.com/499082867`):** Currently uses a privileged
  `agent_process_broker` (`root`) that enforces single-agent execution and
  forwards `ChromotingHostServices`, paired with a single
  `remoting_me2me_host` process running as the logged-in user (or `root` at the
  login screen), pending migration to the multi-process architecture.

### Other Host Modes (It2Me & ChromeOS)
* **Desktop It2Me (Remote Support on Linux, Windows, macOS):** Launched on
  demand as an out-of-process Native Messaging Host (`remote_assistance_host`)
  running as the interactive browser user with explicit user consent dialogs
  and periodic continuation prompts.
* **ChromeOS (`//remoting/host/chromeos/` & `//remoting/client/`):** Both
  `RemoteSupportHostAsh` (interactive It2Me, Enterprise `RemoteCommand`
  unattended support sessions, and Boca/Class Tools) and `RemotingClient` run
  **in-process inside the unsandboxed Ash `chrome` browser binary**. Bugs in
  ChromeOS `//remoting` code or in `//remoting/protocol` reachable on ChromeOS
  therefore execute with Ash browser-process privileges.

### Compromised `peer_connection` Assumption & Rule of Two

In 4-process Me2Me mode, `//remoting` satisfies Chromium's
[Rule of Two](../docs/security/rule-of-2.md) by isolating C++ processing of
untrusted P2P WebRTC packets, media streams, and client data-channel messages
inside the sandboxed `peer_connection` process. Even though `peer_connection`
is spawned post-authentication, this sandbox defends against a compromised or
malicious client application (connecting with valid credentials) as well as
malicious/compromised TURN relays or on-path network attackers sending
malformed ICE/STUN/DTLS/SRTP packets.

* **Defends against a compromised `peer_connection` process: Yes.** Analogous
  to Chromium's compromised-renderer assumption, `daemon`, `network`, and
  `desktop` (which `peer_connection` drives via `DesktopSessionControl`,
  `FileReader`/`FileWriter`, and `VideoCapturer`, and which runs as `SYSTEM` on
  Windows) treat all Mojo IPC messages arriving from `peer_connection` as
  untrusted.
* **`network` is part of the TCB, not an untrusted sandbox:** Running `network`
  as `_crd_network` (Linux) or `LOCAL SERVICE` (Windows) instead of
  `root`/`SYSTEM` is OS least-privilege hardening (for example, Linux
  `IsUidAllowedForDesktopSession()` blocks UID 0 even if `network` were
  compromised). However, because `network` holds the host's identity keys,
  service-account credentials, policy state, and `SessionAuthz` / SPAKE2
  authenticators, `network` is a **trusted control-plane process**.

### Mojo IPC & Boundary Validation

* **`peer_connection` -> `network` (`peer_session.mojom`):**
  `peer_connection` has no direct `DesktopSessionManager` or
  `CertificateBroker` access (and rejects all associated interface requests).
  It can only invoke:
  * `DesktopSessionRequester::RequestDesktopSession(ScreenResolution)`:
    single-call enforced via `mojo::ReportBadMessage()` in
    `IpcPeerSession::RequestDesktopSession()`. `client_id` and
    `required_username` are populated solely by `IpcPeerSession` in `network`.
  * `PairingRequester::RequestPairing`: single-call per session in
    `IpcPeerSession::RequestPairing()`, only wired when pairing is enabled for
    the host.
  * `IceConfigFetcher::GetIceConfig` and
    `TransportEventHandler::SendTransportInfo`: narrow signaling relay methods
    validated in `network`.
* **`peer_connection` <-> `desktop` (`desktop_session.mojom`):**
  Brokered via `desktop_pipe` once `daemon` attaches a desktop process.
  `desktop` is the producer of `ReadOnlySharedMemoryRegion` video frame buffers
  (while audio injection uses a Mojo `data_pipe_consumer`). On Windows (where
  `desktop` runs as `SYSTEM`), `DesktopSessionAgent` delegates input event
  validation to `InputInjector`, while `LocalFileOperations` impersonates the
  logged-in user (`EnsureUserContext()`), strips path traversal via
  `filename.BaseName()`, and delegates file picker UI to the user-token
  `file_chooser` child process.
* **Local User Processes -> `daemon` (`chromoting_host_services.mojom`):**
  `ChromotingHostServicesServer` verifies the connecting OS peer's UID/binary
  (Linux) or WTS session ID/binary (Windows) before routing
  `BindSessionServices` to the matching active remote session.
* **OS Session Eligibility (`daemon`):**
  Even though `network` is trusted, `daemon` enforces OS-level invariants
  before attaching or creating a Linux user session
  (`UserDesktopSessionBackend` / `DesktopSessionLinux`): canonical `passwd`
  username verification, blocking `root` and system UIDs
  (`IsUidAllowedForDesktopSession()`), `/etc/shells` validation, PAM `acct_mgmt`
  (`IsLocalLoginAllowed()`), and `ReadSecureFile()` ownership/mode checks in
  `SessionRoutingConfig::LoadAndValidate()` and `CreatedSessionTracker`.

## Security Boundaries & Explicit Non-Bugs

### In Scope

* **Sandbox escapes / privilege escalation from `peer_connection`:** Bugs
  allowing a compromised `peer_connection` process to execute code in or
  escalate privileges to `network`, `daemon`, or (on Windows) the `SYSTEM`
  `desktop` process, or to access another user's session.
* **Pre-authentication remote vulnerabilities in `network` (or ChromeOS browser
  process):** Memory corruption, authentication bypasses, or unauthorized
  access reachable over signaling (`FtlSignalStrategy`, `CorpSignalStrategy`)
  or session authentication (`SessionAuthz`, SPAKE2, `PairingRegistry`) before
  a client is authenticated, as well as any memory corruption in ChromeOS
  in-browser-process host/client code.
* **Local privilege escalation across OS users:** Bugs allowing an unprivileged
  local user (or a Linux `--type=desktop` / `ChromotingHostServices` process)
  to escalate to `root`/`SYSTEM`, `_crd_network`, or another local user's
  desktop session (e.g., symlink/TOCTOU flaws in privileged file handling or
  IPC peer credential spoofing).
* **Cross-user session routing bypasses:** Bugs allowing an authenticated
  remote client who is only authorized for user `A` to attach to or create a
  session for user `B`.

### Out of Scope (Component-Specific Non-Bugs)

* **Assumed compromise or source-code patching of trusted processes (`daemon`,
  `network`, or Windows `desktop`):** Reports that require arbitrary code
  execution or source-patch injection inside `remoting_me2me_host.cc`
  (`--type=network`), `DaemonProcess` (`--type=daemon`), or Windows
  `DesktopProcess` as a precondition—such as patching `network` to send a
  forged `options.client_id` or `options.required_username` in
  `DesktopSessionManager::GetDesktopSession`—are **not security bugs** unless
  accompanied by a concrete, reachable vulnerability that compromises that
  trusted process first.
* **Missing client-identity re-verification between `network` and `daemon`:**
  `daemon` intentionally omits a network/TLS stack and delegates remote client
  authentication (`SessionAuthz` / SPAKE2) to `network`. Reporting that
  `daemon` trusts `network`-supplied `client_id` or `required_username` without
  re-verifying backend tokens in `daemon` is a non-bug (missing internal
  defense-in-depth across a trusted control-plane channel).
* **Authorized user actions within their own OS session:** Once an
  authenticated and authorized remote user connects to their own session,
  running arbitrary commands, accessing files, or interacting with desktop
  services *as that OS user* is expected remote-desktop functionality.
  Likewise, on Linux, a local user tampering with their own UID's
  `--type=desktop` process is not a privilege escalation unless it crosses into
  `daemon`, `network`, or another user's session. (Note: a Linux `desktop`
  process calling `DesktopSessionRequestHandler::CrashNetworkProcess()`
  currently restarts `network` across all sessions; see `crbug.com/584825017`.
  This is a known host-availability DoS limitation rather than a cross-user
  privilege or confidentiality issue.)
* **Intentional Corp multi-user session creation
  (`createRemoteUserSessions`):** On corporate Linux hosts
  (`RoutingMode::kMatchCorpClientUsername`), omitting `sessionUser` in
  `session_routing.json` when `createRemoteUserSessions` is `true`
  intentionally routes each `SessionAuthz`-authenticated corporate user to
  their matching local account via GDM `CreateUserDisplay` without a second
  interactive password prompt. Keeping CRD-created sessions alive across client
  disconnects and daemon crashes/upgrades (while terminating them on `SIGTERM`
  service stop) is also intended behavior.
* **Safe crashes in `peer_connection`:** `CHECK` failures,
  `mojo::ReportBadMessage()` terminations, or null-pointer dereferences inside
  the sandboxed per-session `peer_connection` process without memory corruption
  only disconnect that session and are treated as functional stability bugs
  rather than security vulnerabilities.

## Design Justifications & Accepted Risks

* **Read-only shared memory for video frames:** `desktop` allocates frame
  buffers and passes `base::ReadOnlySharedMemoryRegion` handles to
  `peer_connection` (`VideoCapturerEventHandler::OnSharedMemoryRegionCreated`)
  for zero-copy encoding. `peer_connection` maps these buffers read-only and
  cannot mutate `desktop` memory; conversely, on Windows `desktop` is trusted
  `SYSTEM`, and on Linux each per-session `peer_connection` process only
  receives region handles for its own session's `desktop` process.
* **Least-privilege control plane (`network`):** Keeping `network` out of
  `root`/`SYSTEM` prevents a hypothetical signaling/TLS bug from immediately
  having root privileges, even though `network` remains inside the host's
  logical TCB because it must hold host identity keys and contact backend authz
  services.
* **World-readable non-secret config files:** On Linux multi-process hosts,
  `DaemonControllerDelegateLinuxMultiProcess`, `SessionRoutingConfig`, and
  `PairingRegistryDelegateLinux` separate secret credentials (`private_key`,
  `oauth_refresh_token`, `pin_hash`, and pairing `sharedSecret`, stored in
  owner-only files readable only by `daemon` or `_crd_network`) from
  world-readable non-secret metadata (`host_unprivileged.json`,
  `session_routing.json`, and `*.unprivileged.json`), with `ReadSecureFile()`
  verifying ownership and write permissions before privileged reads.

## Further Documentation

* [CRD Architecture Overview](docs/architecture.md): Process topology, entry
  points, and Mojo IPC overview.
* [Security for Agents](../docs/security/security-for-agents.md): Chromium-wide
  security boundaries, evidence standards, and universal non-bugs.
* [Mojo IPC Security](../docs/security/mojo.md): Guidelines for safe Mojo
  interfaces across trust boundaries.
