# Security Guidance for //content

The primary security goal of the `//content` module is to safely manage the
multi-process, sandboxed architecture of Chromium, as described in
[content/README.md](https://chromium.googlesource.com/chromium/src/+/main/content/README.md).
This includes enforcing Site Isolation (ensuring that a compromised renderer
process cannot access cross-site data) and the Same-Origin Policy, as well
as coordinating frame trees, process assignment, and navigations (which act as
transitions between security contexts) to prevent sandbox escapes and
privilege escalation.

This document provides guidance for security researchers and AI bug discovery
tools when hunting for vulnerabilities or filing security bugs involving the
`//content` module. It outlines threat model assumptions, common mitigating
factors, areas that are not considered security boundaries, and best practices
for bug reports. If you are an AI agent looking for general Chromium security
guidance across all modules, start with the [Security for
Agents](https://chromium.googlesource.com/chromium/src/+/main/docs/security/security-for-agents.md)
document first.

## Core Documentation
Before analyzing `//content` for security vulnerabilities, please review the
following foundational documents, which explain the core security model for
Chromium, including Site Isolation:
* [Process Model and Site Isolation](https://chromium.googlesource.com/chromium/src/+/main/docs/process_model_and_site_isolation.md)
* [Threat Model and Defenses Against Compromised Renderers](https://chromium.googlesource.com/chromium/src/+/main/docs/security/compromised-renderers.md)

## Threat Model

In general, the `//content` security model assumes that attackers with a
malicious web page can compromise their own renderer process. Many security
goals for the browser should hold even if such an attacker can run arbitrary
code in their renderer process.

However, when considering these attacks, you must assume an attacker can
**only** compromise a process into which they can legitimately load their own
content. Do not assume an attacker can arbitrarily compromise other processes as
a starting condition. While we are very interested in bugs that allow
compromising other processes, you must provide evidence demonstrating exactly
how that happens.

For example, do not assume an attacker can compromise:
* **Spare renderers** (there is no attacker content loaded in them).
* **WebUI processes** (e.g., `chrome://`), though note that
  `chrome-untrusted://` processes *can* be assumed to be compromised (see
  [chrome_untrusted.md](https://chromium.googlesource.com/chromium/src/+/main/docs/webui/chrome_untrusted.md)).
* **Special origin** processes, such as the Chrome Web Store or
  `accounts.google.com`.
* **Component extension** processes such as the PDF extension.
* **Third-Party (3P) Default Search Engine** processes.

Note that it does not make sense to use a compromised higher-privilege process
to compromise a lower-privilege process (e.g., escaping from the GPU process to
compromise a renderer process).

Do not assume the existence of hypothetical, flawed observers in the browser
process. A report describing how a `content/public` API (e.g.,
`WebContentsObserver`) could be misused (e.g., "what if an observer deletes
`this` or makes a re-entrant call into the navigation logic") should be filed as
a Feature Request for hardening security, not as a vulnerability, unless you can
point to a specific, reachable instance of that misuse currently existing in the
Chromium codebase.

### High-Priority Vulnerabilities

Vulnerabilities that break process-level isolation or bypass the sandbox are of
particular interest in `//content`. Some examples of high-value reports are:

* **Sandbox Escapes:** Exploiting a browser process API from a compromised
  renderer to read arbitrary local files (e.g., due to bugs involving file
  upload or drag-and-drop) or execute arbitrary code on the host OS.
* **Privilege Escalation:** Crossing a privileged boundary, such as a
  compromised web renderer gaining access to extension APIs, or a compromised
  `chrome-untrusted://` renderer gaining access to `chrome://` WebUI
  capabilities.
* **Site Isolation Bypasses:**
  * **Process Model Violations:** Forcing a document to commit in an incorrect
    or insufficiently isolated process (e.g., placing a cross-site iframe in the
    parent's process instead of a dedicated process, or injecting unsandboxed
    content into a sandboxed process).
  * **Access to Cross-Site Data:** Allowing a compromised renderer to access
    cross-site data, such as cookies or passwords.
  * **Input Injection:** Allowing a compromised renderer to inject input events
    (especially keyboard events) intended for cross-origin or cross-process
    frames.
  * **Input Interception:** Allowing a compromised renderer to intercept input
    events intended for cross-origin or cross-process frames.

### Lower-Priority Vulnerabilities

Some vulnerabilities represent valid security boundaries but typically have
lower impact or are heavily mitigated. While still worth reporting, these are
generally lower priority. Examples include:

* **Navigations to Disallowed URLs:** Exploiting an IPC or browser feature to
  navigate to privileged URLs (e.g., `chrome://` or `file://`). A common example
  is causing the browser to treat a renderer-initiated navigation as
  browser-initiated.
* **SameSite Cookie Bypasses:** Bypassing `SameSite` cookie protections (which
  are enforced via Site Isolation), typically allowing CSRF-like attacks but not
  direct access to the cookies.
* **Cross-Origin Information Leaks (No Direct Read):** Side channels or limited
  leaks (e.g., sniffing cross-origin layout or detecting whether a cross-origin
  resource is cached) that do not allow direct reading of sensitive cross-site
  data.
* **UI Spoofing via Unvalidated IPCs:** A compromised renderer supplying
  unvalidated strings via IPC to manipulate native browser UI. While spoofing
  the Omnibox or permission prompts is severe, spoofing on surfaces that aren't
  primary security UI surfaces (e.g., file picker dialog titles) is generally
  lower priority.

### Out of scope / Not a Security Boundary

The following areas are generally *not* considered security issues, and
bypasses or issues within them will likely be marked as `WontFix` or treated as
functional bugs (unless a broader boundary is broken):

* **User Gesture / Activation:** Missing user activation checks in the browser
  process are generally not security bugs at this time, because the renderer
  ultimately controls the browser-side user activation state. This is temporary:
  once [ongoing work](https://crbug.com/40091540) to enable trustworthy user
  activation state in the browser process is complete, missing browser-side
  checks will become valid security vulnerabilities.
* **Fenced Frames, Storage Partitioning, Third-Party Cookie Blocking (3PCB):**
  These are examples of privacy features designed to prevent cross-site
  tracking, not process-isolated security boundaries. Because a top-level tab
  and a same-site third-party iframe (or a Fenced Frame) may share the same
  renderer process, a compromised renderer can bypass these restrictions for its
  own site by forging IPCs to appear as though they originate from the
  first-party context. (Note: This is unlike `SameSite` cookies, which *are*
  protected by Site Isolation because they rely on a trustworthy initiator
  origin.)
* **Deprecated Privacy Sandbox Features:** Many Privacy Sandbox features (e.g.,
  Protected Audience, Shared Storage, Fenced Frame Network Revocation) are being
  deprecated and removed. See [this
  page](https://privacysandbox.google.com/overview/status) for a list of
  features and their status. Security bypasses involving these features will be
  deprioritized or WontFixed.
* **Lifecycle States (BFCache / Prerender):** Page lifecycle states are not
  process-isolated security boundaries. If an attacker compromises a renderer,
  escaping the frozen back-forward cache (BFCache) state or activating a
  prerendered page maliciously *within that same process* is not a meaningful
  security boundary bypass, since the attacker already has arbitrary code
  execution in that process. However, it *is* a valid security bug if a
  compromised lifecycle state in one process is used to interfere with an active
  document in a different, cross-site process (e.g., a BFCached page in Process
  A tricking the browser into acting on behalf of an active page in Process B).
* **Safe Browsing:** Safe Browsing is not designed to be protected against a
  compromised renderer process; we can assume that Safe Browsing already failed
  to do its job if a renderer process is compromised.

## Mitigating Factors

When evaluating the severity of a bug, consider what impact it has (e.g., access
to cross-site data, making privileged requests, etc), and whether any mitigating
factors might reduce the severity, such as:
* **Sandboxed Frame Processes:** Compromising the process of a sandboxed frame
  (e.g., via `<iframe sandbox>` or Content Security Policy (CSP) `sandbox`) is
  generally less valuable to an attacker because sandboxed frame processes are
  already forbidden from accessing sensitive site data, such as cookies,
  storage, or passwords.
* **PDF Processes:** Similar to sandboxed frames, compromising a PDF renderer
  process has a lower impact due to its restricted access to site data.
* **MHTML:** MHTML subframes run in heavily locked-down `file:` sandboxed
  processes with scripts disabled. Attacks originating from these restricted
  contexts are generally lower priority.
* **Error pages:** Error pages commit in opaque origins, and main frame error
  pages are assigned to a process locked to `chrome-error://chromewebdata`, with
  no access to site data.
* **User Actions:** Attacks requiring explicit user UI actions (e.g., explicitly
  clicking "Request Desktop Site", dragging and dropping a file to a victim
  origin, or surviving a browser session restore) typically mitigate the
  severity of an issue.
* **Blob URLs:**  Blob URLs contain unguessable UUIDs. Accessing a cross-origin
  Blob URL is only possible if the attacker has already obtained this URL. The
  knowledge of the UUID acts as a capability, and accessing a blob with a known
  UUID is working as intended. Please refer to
  [storage/browser/blob/SECURITY.md](https://chromium.googlesource.com/chromium/src/+/main/storage/browser/blob/SECURITY.md)
  for details on the Blob security model.

## Architectural Rules & Common API Pitfalls

When reviewing code or analyzing vulnerabilities, watch for common
implementation mistakes in these patterns in the code base:
* **Validating Renderer-Supplied URLs and Origins:** The browser process must
  never trust URLs or origins received via IPC. A compromised renderer can forge
  these to bypass Site Isolation or access privileged schemes.
  `ChildProcessSecurityPolicy` (e.g., `CanAccessDataForOrigin()`) and
  `FilterURL()` are commonly used for this.
* **Using the full `SecurityPrincipal` / `SiteInfo`**: When making process
  isolation decisions (e.g., determining if a process is suitable for a given
  document), relying solely on a URL or Origin is often insufficient. Two
  documents might share the same origin but still need to be isolated in
  separate processes (e.g., due to `<iframe sandbox>`, `is_pdf`,
  [Document-Isolation-Policy](https://wicg.github.io/document-isolation-policy/),
  or being in different `StoragePartition`s). These decisions should instead use
  the full `SecurityPrincipal` (implemented as `SiteInfo`, more information
  [here](https://chromium.googlesource.com/chromium/src/+/main/docs/process_model_and_site_isolation.md#abstractions-and-implementations)),
  and consider all of its members. Note: This applies specifically to process
   model decisions; standard origin checks (e.g., `IsSameOriginWith` for CORS)
   do not necessarily require `SiteInfo`.
* **Inactive RenderFrameHosts:** While inactive `RenderFrameHost` instances
  (i.e., those in the BFCache or pending deletion) must still process certain
  IPCs (e.g., routing `postMessage` sent from `unload` handlers), the browser
  process should avoid performing security-sensitive state changes or
  establishing new frame relationships. A common pitfall is handling an IPC by
  looking up the `FrameTreeNode` and acting on its `current_frame_host()`. If
  the IPC was sent by an inactive `RenderFrameHost` (e.g., pending deletion),
  acting on `current_frame_host()` may unintentionally apply the action to the
  *new*, potentially cross-origin document.

## Terminology and Categorization

Please be precise when describing vulnerabilities:
* **Sandboxed frame** bypasses are not **Sandbox Escapes**: Escaping a CSP
  `sandbox` or `<iframe sandbox>` is a Site Isolation / origin boundary issue,
  *not* an OS-level sandbox escape (which refers to escaping the Chromium
  renderer sandbox to execute code on the host OS).
* Don't confuse **Storage Partitioning** (the Privacy Sandbox concept) and the
  **`content::StoragePartition`** class. There is no security boundary for the
  former, but there is one for the latter: a compromised renderer should not be
  able to access a different `content::StoragePartition`.
  `content::StoragePartition` is closer to a different `Profile` and gets used
  for cases like GuestViews and Isolated Web Apps.
* **Universal Cross-Site Scripting (UXSS) claims:** Be clear whether a "UXSS"
  claim is an actual **Site Isolation bypass** (e.g., injecting scripts into a
  cross-process, cross-site frame) or merely a bypass within the same renderer
  process. True Site Isolation bypasses are significantly more severe.
