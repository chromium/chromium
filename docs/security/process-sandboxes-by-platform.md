# Unsandboxed Processes by Platform

This document summarises the sandboxes used for different processes or services
in Chrome. This informs the [severity of security
issues](../../docs/security/severity-guidelines.md) in different processes.
Security issues are triaged based on the least-sandboxed platform where an issue
occurs. Some processes may be sandboxed but contain important credentials or
cross-origin data, for this table they count as being sandboxed.

This table will be updated to track the default configuration of the Stable
Chrome channel (i.e. 100% of clients adopt the tighter configuration).

The utility process type hosts several services with different sandboxing
requirements. Find the sandbox used by a utility by finding the
[`ServiceSandbox` attribute](../../sandbox/policy/mojom/sandbox.mojom) used in
its main mojo service.

Last updated for M128.

# Not sandboxed on some platforms

| Process / Service | Platform(s) | Sandbox |
|---|---|---|
| Browser | all | **unsandboxed** |
| Network | Android, Windows, Linux | **unsandboxed** |
| GPU | Android, non-ChromeOS Linux | **unsandboxed** |
| On Device Model Execution | Android, non-ChromeOS Linux | **unsandboxed** |
| Video Capture | non-Fuchsia | **unsandboxed** |
| kNoSandbox | all | **unsandboxed** |
| kNoSandboxAndElevatedPrivileges | Windows | **Elevated** |

# Sandboxed on specific platforms

* kNetwork (Fuchsia, Mac)
* kGpu (Fuchsia, Mac, Windows, ChromeOS)
* kVideoCapture (Fuchsia)

# Sandboxed

* kRenderer (renderer, extensions, PDF renderers)
* kUtility
* kService
* kServiceWithJit
* kAudio
* kOnDeviceModelExecution
* kCdm
* kPrintCompositor
* kSpeechRecognition
* kScreenAI
* kPpapi
* kPrintBackend
* kVideoCapture (Fuchsia only)
* kIconReader (Windows only)
* kMediaFoundationCdm (Windows only)
* kPdfConversion (Windows only)
* kXrCompositing (Windows only)
* kWindowsSystemProxyResolver (Windows only)
* kHardwareVideoDecoding (Linux & Ash)
* kHardwareVideoEncoding (Linux & Ash)
* kIme (Ash only)
* kTts (Ash only)
* kLibassistant (Ash only)
* kNearby (Ash only)
* kMirroring (MacOS only)
