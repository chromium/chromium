## Why Graphics?

The GPU process is interesting from an attacker perspective for several reasons.

1. Many of its features are reachable directly from web content by default,
   which creates an opportunity for malicious websites to attack Chromium users.
2. It processes complex data in (mostly) C++ native code, which is difficult to
   do safely.
3. It needs the privilege to interact with GPU drivers in the kernel, so our
   ability to sandbox the process is limited.
4. It loads third party native code into its address space to interact with
   platform specific graphics features.

Collectively these properties make the GPU process particularly attractive for
both remote code execution and privilege escalation.
