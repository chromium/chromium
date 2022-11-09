// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/scoped_va_config.h"

#include <va/va_str.h>

#include "media/gpu/vaapi/test/macros.h"
#include "media/gpu/vaapi/test/vaapi_device.h"

namespace media {
namespace vaapi_test {

ScopedVAConfig::ScopedVAConfig(const VaapiDevice& device,
                               VAProfile profile,
                               unsigned int va_rt_format)
    : device_(device),
      config_id_(VA_INVALID_ID),
      profile_(profile),
      va_rt_format_(va_rt_format) {
  // We rely on vaCreateConfig to specify the error mode if decode is not
  // supported for the given profile.
  std::vector<VAConfigAttrib> attribs;
  attribs.push_back(
      {VAConfigAttribRTFormat, base::strict_cast<uint32_t>(va_rt_format_)});
  const VAStatus res =
      vaCreateConfig(device_->display(), profile_, VAEntrypointVLD,
                     attribs.data(), attribs.size(), &config_id_);
  VA_LOG_ASSERT(res, "vaCreateConfig");
  LOG_ASSERT(config_id_ != VA_INVALID_ID)
      << "vaCreateConfig created invalid config ID";
  VLOG(1) << "Created config with ID " << config_id_ << " and profile "
          << vaProfileStr(profile_);
}

ScopedVAConfig::~ScopedVAConfig() {
  VLOG(1) << "Destroying config " << config_id_ << " with profile "
          << vaProfileStr(profile_);
  DCHECK_NE(config_id_, VA_INVALID_ID);
  const VAStatus res = vaDestroyConfig(device_->display(), config_id_);
  VA_LOG_ASSERT(res, "vaDestroyConfig");
}

}  // namespace vaapi_test
}  // namespace media
