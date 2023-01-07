# WebXR Blink Module
_For a more thorough/high level overview of the entire WebXR stack, please refer to
[components/webxr](https://source.chromium.org/chromium/chromium/src/+/main:components/webxr/README.md)_

The WebXR API enables Virtual Reality (VR) and Augmented Reality (AR) features on the Web.

WebXR and it's associated modules are developed by the Immersive Web W3C
[Working Group](https://www.w3.org/immersive-web/) and [Community Group](https://www.w3.org/community/immersive-web/)

This Blink module implements the "core" [WebXR Device API](https://www.w3.org/TR/webxr/), as well as the following
WebXR modules:

 - [Gamepads](https://www.w3.org/TR/webxr-gamepads-module-1/)
 - [Augmented Reality](https://www.w3.org/TR/webxr-ar-module-1/)
 - [Hit Test](https://immersive-web.github.io/hit-test/)
 - [DOM Overlays](https://immersive-web.github.io/dom-overlays/)
 - [Anchors](https://immersive-web.github.io/anchors/)
 - [Lighting Estimation](https://immersive-web.github.io/lighting-estimation/)

The Blink interfaces are supported by the backends implemented in
[chrome/browser/vr/](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/vr) and
[device/vr/](https://source.chromium.org/chromium/chromium/src/+/main:device/vr)
