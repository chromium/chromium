# Remoting Host Mojo Interfaces

This directory defines the Mojo interfaces for IPC between CRD host processes.

## Process Communication Map

Understanding which processes use which interfaces is key to navigating the host
architecture.

*   **`remoting_host.mojom`**
    *   *Users:* `Network` <-> `Daemon`
    *   *Role:* High-level host control, configuration, certificate brokering,
        and status.
*   **`peer_session.mojom`**
    *   *Users:* `Daemon` / `Network` <-> `PeerConnection`
    *   *Role:* Spawning per-session `PeerConnection` processes and narrow
        control/signaling relay across the untrusted boundary.
*   **`desktop_session.mojom`**
    *   *Users:* `Daemon` <-> `Network` (`DesktopSessionManager`), and Session
        Owner (`PeerConnection`) <-> `Desktop`
    *   *Role:* Desktop session brokering, screen/audio capture, input
        injection, and file transfer.
*   **`chromoting_host_services.mojom`**
    *   *Users:* `UserProcess` -> `Daemon` (`ChromotingHostServices`) -> Session
        Owner (`PeerConnection` or single-process host,
        `ChromotingSessionServices`)
    *   *Role:* Allows user-level processes (like `remote-open-url` and
        `remote-webauthn`) to bind session-scoped services after UID/session
        verification.
*   **`remote_url_opener.mojom`**
    *   *Users:* Session Owner (`PeerConnection` or single-process host) <->
        `RemoteOpenUrl`
    *   *Role:* URL forwarding from host session to client.
*   **`webauthn_proxy.mojom`**
    *   *Users:* Session Owner (`PeerConnection` or single-process host) <->
        `WebAuthn`
    *   *Role:* WebAuthn request/response proxying.
*   **`remote_security_key.mojom`**
    *   *Users:* `RemoteSecurityKey` (Windows) or `Desktop` (POSIX) <-> Session
        Owner (`PeerConnection` or single-process host)
    *   *Role:* Security-key (gnubby/SSH agent) request/response forwarding.
*   **`login_session.mojom`**
    *   *Users:* `RemoteSessionInfo` (Linux user helper) <-> `Daemon`
    *   *Role:* Linux-specific session login management and reporting.

## Important Guidelines

1.  **Security Review:** Any changes to Mojo interfaces that span process
    boundaries MUST be reviewed for security.
2.  **Versioning:** All binaries are versioned together so it is OK to change
    the mojom in ways which would constitute a breaking change otherwise.
3.  **Broker Process:** The `Daemon` process is typically the Mojo Broker. All
    initial Mojo connections should be established through it.
4.  **Async Design:** Use `OnceCallback` and `Remote` carefully. Most host IPC
    is asynchronous to prevent blocking the network thread.

## How to Extend

To add a new process type:

1.  Define a new `.mojom` interface here.
2.  Update `chromoting_host_services.mojom` if the new process needs to be
    accessible from other user processes.
3.  Implement the interface in the appropriate `remoting/host/` implementation
    file.

## Unit Testing Traits

When adding or modifying `StructTraits`, `EnumTraits`, or `UnionTraits` in
`remoting_mojom_traits.h`, you MUST add or update the corresponding tests in
`remoting_mojom_traits_unittest.cc`. These tests ensure that the mapping between
C++ types and Mojom types is correct and remains stable.

Example test pattern:
```cpp
TEST(RemotingMojomTraitsTest, MyType) {
  MyType input = ...;
  MyType output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::MyType>(input, output));
  EXPECT_EQ(input, output);
}
```
