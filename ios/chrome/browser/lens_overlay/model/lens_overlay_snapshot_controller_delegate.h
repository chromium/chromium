// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_SNAPSHOT_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_SNAPSHOT_CONTROLLER_DELEGATE_H_

class LensOverlaySnapshotControllerDelegate {
 public:
  LensOverlaySnapshotControllerDelegate() = default;
  LensOverlaySnapshotControllerDelegate(
      const LensOverlaySnapshotControllerDelegate&) = delete;
  LensOverlaySnapshotControllerDelegate& operator=(
      const LensOverlaySnapshotControllerDelegate&) = delete;
  virtual ~LensOverlaySnapshotControllerDelegate() = default;

  virtual void OnSnapshotCaptureBegin() = 0;
  virtual void OnSnapshotCaptureEnd() = 0;
};

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_SNAPSHOT_CONTROLLER_DELEGATE_H_
