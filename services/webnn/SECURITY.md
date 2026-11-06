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
| Job memory limit | 1 TB | 1 TB |

## Compiled model validation at the Compiler-to-GPU boundary

The Compiler process is treated as untrusted: it may be compromised by
the graph it compiles. The compiled model it sends to the GPU process
over `WebNNModelLoader` (`public/mojom/webnn_model_loader.mojom`) is
therefore validated before it is loaded.

For execution providers that compile a graph into a single EPContext
node, the compiled ONNX model is expected to be just a wrapper around
the EP's precompiled blob, and the validator
(`ort/epcontext_model_validator.h`) only confirms that shape:

- It must contain exactly one graph.
- The graph must contain exactly one node.
- That single node must be an EPContext (`com.microsoft`) node.

This is intentional. A model of this shape passing straight through the
validator is the expected outcome, not a gap. The check exists only to
stop a compromised Compiler from smuggling extra ONNX nodes past the
boundary for ORT to run in the GPU process. The EPContext node's opaque
blob is deliberately not parsed here; it is in a vendor-specific format,
and validating it is the responsibility of the execution provider and
its backend.

Validation is per execution provider, selected by `compiled_model_format`
in `public/cpp/execution_providers_info.h`. Only execution providers
declaring `kEpContext` are validated. An execution provider declaring
`kFullGraph` compiles into an ordinary ONNX model, which has no such
invariant to check, and is loaded as produced; its safety rests on
ORT's own parsing. Every enabled execution provider must declare its
format, which a `static_assert` enforces, so enabling one cannot leave
it loaded unvalidated by omission. When enabling an execution provider,
declare `kEpContext` if it compiles into an EPContext node.

The model bytes may arrive in shared memory the Compiler can still
write, so they are moved into private memory before validation; the
bytes validated are the bytes ORT loads.
