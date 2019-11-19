// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_process_sandbox_support_impl_mac.h"

#include <utility>

#include "base/bind.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "content/common/mac/font_loader.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

WebSandboxSupportMac::WebSandboxSupportMac() {
  if (auto* thread = ChildThread::Get()) {
    thread->BindHostReceiver(sandbox_support_.BindNewPipeAndPassReceiver());
    sandbox_support_->GetSystemColors(base::BindOnce(
        &WebSandboxSupportMac::OnGotSystemColors, base::Unretained(this)));
  }
}

WebSandboxSupportMac::~WebSandboxSupportMac() = default;

bool WebSandboxSupportMac::LoadFont(CTFontRef font,
                                    CGFontRef* out,
                                    uint32_t* font_id) {
  if (!sandbox_support_)
    return false;
  base::ScopedCFTypeRef<CFStringRef> name_ref(CTFontCopyPostScriptName(font));
  base::string16 font_name = SysCFStringRefToUTF16(name_ref);
  float font_point_size = CTFontGetSize(font);
  mojo::ScopedSharedBufferHandle font_data;
  bool success = sandbox_support_->LoadFont(font_name, font_point_size,
                                            &font_data, font_id) &&
                 *font_id > 0 && font_data.is_valid();
  if (!success) {
    DLOG(ERROR) << "Bad response from LoadFont() for " << font_name;
    *out = nullptr;
    *font_id = 0;
    return false;
  }

  uint64_t font_data_size = font_data->GetSize();
  DCHECK_GT(font_data_size, 0U);
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(font_data_size));

  // TODO(jeremy): Need to call back into the requesting process to make sure
  // that the font isn't already activated, based on the font id.  If it's
  // already activated, don't reactivate it here - https://crbug.com/72727 .
  return FontLoader::CGFontRefFromBuffer(
      std::move(font_data), static_cast<uint32_t>(font_data_size), out);
}

SkColor WebSandboxSupportMac::GetSystemColor(blink::MacSystemColorID color_id) {
  if (!color_map_.IsValid()) {
    DLOG(ERROR) << "GetSystemColor does not have a valid color_map_";
    return SK_ColorMAGENTA;
  }
  base::span<const SkColor> color_map =
      color_map_.GetMemoryAsSpan<SkColor>(blink::kMacSystemColorIDCount);
  return color_map[static_cast<size_t>(color_id)];
}

void WebSandboxSupportMac::OnGotSystemColors(
    base::ReadOnlySharedMemoryRegion region) {
  color_map_ = region.Map();
}

}  // namespace content
