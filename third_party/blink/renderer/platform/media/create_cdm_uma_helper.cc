// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/create_cdm_uma_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "media/base/cdm_config.h"
#include "media/base/key_systems.h"

namespace blink {

std::string GetUMAPrefixForCdm(const media::CdmConfig& cdm_config) {
  auto key_system_name_for_uma = media::GetKeySystemNameForUMA(
      cdm_config.key_system, cdm_config.use_hw_secure_codecs);
  auto key_system_uma_prefix = "Media.EME." + key_system_name_for_uma + ".";
  return key_system_uma_prefix;
}

void ReportCreateCdmStatusUMA(const std::string& uma_prefix,
                              bool is_cdm_created,
                              media::CreateCdmStatus status) {
  DCHECK(uma_prefix.ends_with("."));
  base::UmaHistogramBoolean(uma_prefix + "CreateCdm", is_cdm_created);
  base::UmaHistogramEnumeration(uma_prefix + "CreateCdmStatus", status);
}

void ReportCreateCdmTimeUMA(const std::string& uma_prefix,
                            base::TimeDelta delta) {
  DCHECK(uma_prefix.ends_with("."));
  base::UmaHistogramTimes(uma_prefix + "CreateCdmTime", delta);
}

}  // namespace blink
