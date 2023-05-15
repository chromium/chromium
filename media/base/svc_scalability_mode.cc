// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/svc_scalability_mode.h"

#include "base/notreached.h"

namespace media {

const char* GetScalabilityModeName(SVCScalabilityMode scalability_mode) {
  switch (scalability_mode) {
    case SVCScalabilityMode::kL1T1:
      return "L1T1";
    case SVCScalabilityMode::kL1T2:
      return "L1T2";
    case SVCScalabilityMode::kL1T3:
      return "L1T3";
    case SVCScalabilityMode::kL2T1:
      return "L2T1";
    case SVCScalabilityMode::kL2T2:
      return "L2T2";
    case SVCScalabilityMode::kL2T3:
      return "L2T3";
    case SVCScalabilityMode::kL3T1:
      return "L3T1";
    case SVCScalabilityMode::kL3T2:
      return "L3T2";
    case SVCScalabilityMode::kL3T3:
      return "L3T3";
    case SVCScalabilityMode::kL2T1h:
      return "L2T1h";
    case SVCScalabilityMode::kL2T2h:
      return "L2T2h";
    case SVCScalabilityMode::kL2T3h:
      return "L2T3h";
    case SVCScalabilityMode::kS2T1:
      return "S2T1";
    case SVCScalabilityMode::kS2T2:
      return "S2T2";
    case SVCScalabilityMode::kS2T3:
      return "S2T3";
    case SVCScalabilityMode::kS2T1h:
      return "S2T1h";
    case SVCScalabilityMode::kS2T2h:
      return "S2T2h";
    case SVCScalabilityMode::kS2T3h:
      return "S2T3h";
    case SVCScalabilityMode::kS3T1:
      return "S3T1";
    case SVCScalabilityMode::kS3T2:
      return "S3T2";
    case SVCScalabilityMode::kS3T3:
      return "S3T3";
    case SVCScalabilityMode::kS3T1h:
      return "S3T1h";
    case SVCScalabilityMode::kS3T2h:
      return "S3T2h";
    case SVCScalabilityMode::kS3T3h:
      return "S3T3h";
    case SVCScalabilityMode::kL2T1Key:
      return "L2T1_KEY";
    case SVCScalabilityMode::kL2T2Key:
      return "L2T2_KEY";
    case SVCScalabilityMode::kL2T2KeyShift:
      return "L2T2_KEY_SHIFT";
    case SVCScalabilityMode::kL2T3Key:
      return "L2T3_KEY";
    case SVCScalabilityMode::kL2T3KeyShift:
      return "L2T3_KEY_SHIFT";
    case SVCScalabilityMode::kL3T1Key:
      return "L3T1_KEY";
    case SVCScalabilityMode::kL3T2Key:
      return "L3T2_KEY";
    case SVCScalabilityMode::kL3T2KeyShift:
      return "L3T2_KEY_SHIFT";
    case SVCScalabilityMode::kL3T3Key:
      return "L3T3_KEY";
    case SVCScalabilityMode::kL3T3KeyShift:
      return "L3T3_KEY_SHIFT";
  }
  NOTREACHED_NORETURN();
}
}  // namespace media
