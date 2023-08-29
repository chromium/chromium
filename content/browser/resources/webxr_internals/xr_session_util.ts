// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {XRDepthDataFormat, XRDepthUsage, XRSessionFeature, XRSessionMode} from './xr_session.mojom-webui.js';

export function depthFormatToString(format: XRDepthDataFormat): string {
  switch (format) {
    case XRDepthDataFormat.kLuminanceAlpha:
      return 'luminance-alpha';
    case XRDepthDataFormat.kFloat32:
      return 'float32';
    default:
      return '';
  }
}

export function depthUsageToString(usage: XRDepthUsage): string {
  switch (usage) {
    case XRDepthUsage.kCPUOptimized:
      return 'CPU';
    case XRDepthUsage.kGPUOptimized:
      return 'GPU';
    default:
      return '';
  }
}

export function sessionFeatureToString(feature: XRSessionFeature): string {
  switch (feature) {
    case XRSessionFeature.REF_SPACE_VIEWER:
      return 'viewer';
    case XRSessionFeature.REF_SPACE_LOCAL:
      return 'local';
    case XRSessionFeature.REF_SPACE_LOCAL_FLOOR:
      return 'local-floor';
    case XRSessionFeature.REF_SPACE_BOUNDED_FLOOR:
      return 'bounded-floor';
    case XRSessionFeature.REF_SPACE_UNBOUNDED:
      return 'unbounded';
    case XRSessionFeature.DOM_OVERLAY:
      return 'dom-overlay';
    case XRSessionFeature.HIT_TEST:
      return 'hit-test';
    case XRSessionFeature.LIGHT_ESTIMATION:
      return 'light-estimation';
    case XRSessionFeature.ANCHORS:
      return 'anchors';
    case XRSessionFeature.CAMERA_ACCESS:
      return 'camera-access';
    case XRSessionFeature.PLANE_DETECTION:
      return 'plane-detection';
    case XRSessionFeature.DEPTH:
      return 'depth-sensing';
    case XRSessionFeature.IMAGE_TRACKING:
      return 'image-tracking';
    case XRSessionFeature.HAND_INPUT:
      return 'hand-tracking';
    case XRSessionFeature.SECONDARY_VIEWS:
      return 'secondary-views';
    case XRSessionFeature.LAYERS:
      return 'layers';
    case XRSessionFeature.FRONT_FACING:
      return 'front-facing';
    default:
      return '';
  }
}

export function sessionModeToString(mode: XRSessionMode): string {
  switch (mode) {
    case XRSessionMode.kInline:
      return 'inline';
    case XRSessionMode.kImmersiveVr:
      return 'immersive-vr';
    case XRSessionMode.kImmersiveAr:
      return 'immersive-ar';
    default:
      return '';
  }
}
