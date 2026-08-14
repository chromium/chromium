# WebXR Mojom Test Interfaces

## Introduction

This directory contains the Mojom test interfaces ([`xr_test_hook.test-mojom`](xr_test_hook.test-mojom)),
the C++ structs to which those definitions are typemapped, and their struct traits
([`xr_test_hook_mojom_traits.h`](xr_test_hook_mojom_traits.h)).

General WebXR test documentation can be found in
[`//chrome/browser/vr/test/xr_browser_tests.md`](../../../../../chrome/browser/vr/test/xr_browser_tests.md).

## Decoupling Test Interfaces from Production Code

The Mojom target `test_mojom`, component `vr_public_test_typemaps`, and utility
target `vr_test_utils` are all marked **`testonly = true`**. No production targets
depend on the test Mojom or its typemapped types.

On Windows, the OpenXR runtime executes in a sandboxed utility process (the
`isolated_xr_device` service). To connect the test process's `MockXRDeviceHookBase`
to the mock OpenXR runtime in the utility process without introducing test-only
Mojom dependencies into production code:

1. The production interface [`isolated_xr_service.mojom`](../isolated_xr_service.mojom)
   defines `BindHookForTesting(handle<message_pipe> receiver)` using a raw,
   type-erased message pipe handle.
2. `XRDeviceService::BindHookForTesting` passes this handle to
   `device::OpenXrPlatformHelper::BindHookForTesting`.
3. In test builds linking `//device/vr:openxr_test_helper`, static initialization
   instantiates a registrar (`TrampolineRegistrar` in `openxr_mock_helper.cc`) that
   registers function pointers with `OpenXrPlatformHelper`.
4. The registered `BindTestHook` function unwraps the pipe handle into a typed
   `mojo::PendingRemote<device_test::mojom::XRTestHook>` and binds it to
   `OpenXrTestHelper::Get().SetTestHook(...)`.

On Android, device code runs in-process with the browser tests, so
`MockXRDeviceHookBase` directly passes its `mojo::PendingRemote` to
`OpenXrTestHelper::Get()` without IPC.

## Mock Runtime Binding

The other end of this Mojom pipe is bound to the mock OpenXR runtime helper
(`OpenXrTestHelper`), which is embedded directly into the host process and receives
calls via a thin trampoline. For details on how the mock runtime is initialized and
intercepts OpenXR loader calls, see the
[OpenXR Testing documentation](../../../openxr/README.md#testing).
