# Accessibility Service

The accessibility service on Chrome OS provides accessibility services like
ChromeVox, Select-to-Speak, Switch Access and Dictation, and a framework API
to communicate with the operating system. On Chrome desktop, the
accessibility service could be used by native APIs to expose accessibility
information to the operating system.

## Chrome OS

On Chrome OS, the service runs a V8 instance which will execute Accessibility
feature Javascript. The V8 implementation is in features/.
