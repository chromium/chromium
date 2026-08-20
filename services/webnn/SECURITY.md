# WebNN Compiler process vs. GPU process sandboxes

WebNN inference runs in the GPU process. On Windows, when the
`WebNNCompilerProcess` feature is enabled, graph compilation is moved
out into a separate **Compiler process** so that third-party ML
runtime code (ONNX Runtime plus a vendor execution provider) parses
and transforms attacker-controlled graphs outside the GPU process.

The two sandboxes have different policies: the Compiler process is
substantially more restricted, and in particular it has no access to
graphics or NPU device objects, while the GPU process does.

This document describes Windows only, which is the only platform with
a separate Compiler process today
(`sandbox.mojom.Sandbox.kWebNNModelCompilation` is
`[EnableIf=is_win]`, and `webnn_compiler_service.mojom` is only built
when `is_win`). Elsewhere, compilation still happens in the GPU
process.

## Comparison

| | Compiler process (`kWebNNModelCompilation`) | GPU process (`kGpu`) |
|---|---|---|
| AppContainer | LPAC, profile prefix `cr.sb.wnn` | N/A |
| LPAC capabilities | `lpacChromeInstallFiles`, `registryRead`; `chromeInstallFiles` as an impersonation capability only | N/A |
| Initial token | `USER_RESTRICTED_SAME_ACCESS` | `USER_RESTRICTED_SAME_ACCESS` |
| Lockdown token | `USER_LOCKDOWN` | `USER_LIMITED` |
| Integrity level | N/A | `INTEGRITY_LEVEL_LOW` |
| Job level | `JobLevel::kLockdown` | `JobLevel::kLimitedUser` with `SYSTEMPARAMETERS`, `DESKTOP`, `EXITWINDOWS`, `DISPLAYSETTINGS` UI exceptions |
| win32k | `MITIGATION_WIN32K_DISABLE` as a pre-launch mitigation, so the kernel rejects win32k syscalls from the child's first instruction | Not disabled |
| Dynamic code (ACG) | `MITIGATION_DYNAMIC_CODE_DISABLE` | Allowed |
| Code Integrity Guard | Startup CIG via `ChromeContentBrowserClient::PreSpawnChild()`, plus the delayed `MITIGATION_FORCE_MS_SIGNED_BINS` | Delayed `MITIGATION_FORCE_MS_SIGNED_BINS` only |
| Default DACL | `SetLockdownDefaultDacl()` | `SetLockdownDefaultDacl()` + `AddRestrictingRandomSid()` |
| Job memory limit | 1 TB | 1 TB with `kWinSboxHighGPUJobMemoryLimits` |
