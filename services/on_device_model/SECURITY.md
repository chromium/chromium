# Security Considerations for On-Device Model Service

This document outlines key security considerations for the On-Device Model Service (located in `//services/on_device_model`).

## Privilege and Process Isolation

This code executes in a process that is shared between all sites and Chrome built-in features.

**Therefore, compromising this process could lead to cross-site information disclosure.**
