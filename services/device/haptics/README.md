# Haptics

This directory contains the Device Service implementation of the
[Web Haptics API][wicg-explainer], providing platform-specific actuator control
for semantic tactile feedback (`hint`, `edge`, `tick`, `align`, `success`, and
`error`).

## Code Map

- [`blink::HapticsController`][controller]: Implements
  `navigator.playHaptics()` in Blink and communicates with the browser process
  over [`device.mojom.HapticsService`][haptics-mojom].
- [`content::HapticsServiceImpl`][service-impl]: Per-document browser host that
  validates requests before forwarding them to the platform backend.
- [`device.mojom.HapticsService`][haptics-mojom]: Shared Mojo interface bound
  in the Device Service via `DeviceService::BindHapticsService` for triggering
  platform haptics.
- [`device::HapticsManagerImpl`][manager-impl]: Top-level Device Service class
  that implements `device.mojom.HapticsService` and dispatches `PlayHaptics()`
  to `PlatformPlayHaptics()`.

## Security and Permissions

The Device Service performs no origin, permission, or user-activation checks;
`DeviceService::BindHapticsService` is exposed only to the browser process.
When a renderer binds `device.mojom.HapticsService`,
[`content::HapticsServiceImpl`][service-impl] rejects fenced frames and frames
blocked by `PermissionsPolicyFeature::kHaptics`, validates finite intensity in
`[0.0, 1.0]`, and requires an active frame with sticky user activation.

## Platform Support

- **Windows**: Currently implemented in `//content/browser/haptics/` via
  [`content::HapticsManagerImplWin`][manager-win].
- **Other platforms**: `HapticsManagerImpl::PlatformPlayHaptics()` defaults to
  a `NOTIMPLEMENTED()` stub.

## Testing

- Device Service unit tests:
  [`haptics_manager_impl_unittest.cc`][impl-unittest] (`services_unittests`).
- Browser tests: [`haptics_service_impl_browsertest.cc`][browsertest]
  (`content_browsertests`).

## Design Documents

- [WICG Web Haptics Explainer][wicg-explainer]
- [Web Haptics Windows Design Document][win-design-doc]

[wicg-explainer]: https://github.com/WICG/web-haptics
[controller]: /third_party/blink/renderer/modules/haptics/haptics_controller.h
[haptics-mojom]: /services/device/public/mojom/haptics.mojom
[service-impl]: /content/browser/haptics/haptics_service_impl.h
[manager-impl]: haptics_manager_impl.h
[manager-win]: /content/browser/haptics/haptics_manager_impl_win.h
[impl-unittest]: haptics_manager_impl_unittest.cc
[browsertest]: /content/browser/haptics/haptics_service_impl_browsertest.cc
[win-design-doc]: https://github.com/kyerebo/chromium-design-docs/blob/56e5aa29f02d2171bc2c5f648ba37be346b81a6f/web-haptics/web-haptics-design-doc.md
