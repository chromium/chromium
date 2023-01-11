// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_process_sandbox_support_impl_mac.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/strings/sys_string_conversions.h"
#include "content/common/mac/font_loader.h"
#include "content/public/child/child_thread.h"

namespace content {

WebSandboxSupportMac::WebSandboxSupportMac() {
  if (auto* thread = ChildThread::Get()) {
    thread->BindHostReceiver(sandbox_support_.BindNewPipeAndPassReceiver());
    sandbox_support_->GetSystemColors(base::BindOnce(
        &WebSandboxSupportMac::OnGotSystemColors, base::Unretained(this)));
  }
}

WebSandboxSupportMac::~WebSandboxSupportMac() = default;

bool WebSandboxSupportMac::LoadFont(
    CTFontRef font,
    base::ScopedCFTypeRef<CTFontDescriptorRef>* out_descriptor,
    uint32_t* font_id) {
  if (!sandbox_support_)
    return false;
  base::ScopedCFTypeRef<CFStringRef> name_ref(CTFontCopyPostScriptName(font));
  std::u16string font_name = SysCFStringRefToUTF16(name_ref);
  float font_point_size = CTFontGetSize(font);
  base::ReadOnlySharedMemoryRegion font_data;
  bool success = sandbox_support_->LoadFont(font_name, font_point_size,
                                            &font_data, font_id) &&
                 *font_id > 0 && font_data.IsValid();
  if (!success) {
    DLOG(ERROR) << "Bad response from LoadFont() for " << font_name;
    out_descriptor->reset();
    *font_id = 0;
    return false;
  }

  size_t font_data_size = font_data.GetSize();
  DCHECK_GT(font_data_size, 0U);

  // TODO(jeremy): Need to call back into the requesting process to make sure
  // that the font isn't already activated, based on the font id.  If it's
  // already activated, don't reactivate it here - https://crbug.com/72727 .
  return FontLoader::CTFontDescriptorFromBuffer(std::move(font_data),
                                                out_descriptor);
}

SkColor WebSandboxSupportMac::GetSystemColor(
    blink::MacSystemColorID color_id,
    blink::mojom::ColorScheme color_scheme) {
  if (!color_map_.IsValid()) {
    DLOG(ERROR) << "GetSystemColor does not have a valid color_map_";
    return SK_ColorMAGENTA;
  }
  static_assert(blink::kMacSystemColorSchemeCount == 2,
                "Light and dark color scheme system colors loaded.");
  base::span<const SkColor> color_map = color_map_.GetMemoryAsSpan<SkColor>(
      blink::kMacSystemColorIDCount * blink::kMacSystemColorSchemeCount);
  base::span<const SkColor> color_map_for_scheme =
      color_map.subspan(color_scheme == blink::mojom::ColorScheme::kDark
                            ? blink::kMacSystemColorIDCount
                            : 0,
                        blink::kMacSystemColorIDCount);
  return color_map_for_scheme[static_cast<size_t>(color_id)];
}

void WebSandboxSupportMac::OnGotSystemColors(
    base::ReadOnlySharedMemoryRegion region) {
  color_map_ = region.Map();
}

}  // namespace content
