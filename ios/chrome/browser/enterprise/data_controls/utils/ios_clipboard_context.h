// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_UTILS_IOS_CLIPBOARD_CONTEXT_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_UTILS_IOS_CLIPBOARD_CONTEXT_H_

#import "base/memory/raw_ptr.h"
#import "components/enterprise/data_controls/core/browser/clipboard_context.h"
#import "ui/base/clipboard/clipboard_metadata.h"

class ProfileIOS;

namespace data_controls {

class IOSClipboardContext : public ClipboardContext {
 public:
  IOSClipboardContext(const GURL& source_url,
                      const GURL& destination_url,
                      ProfileIOS* source_profile,
                      ProfileIOS* destination_profile,
                      ui::ClipboardMetadata metadata);

  // ClipboardContext override:
  GURL source_url() const override;
  GURL destination_url() const override;
  enterprise_connectors::ContentMetaData::CopiedTextSource
  data_controls_copied_text_source() const override;
  ui::ClipboardFormatType format_type() const override;
  std::optional<size_t> size() const override;
  std::string source_active_user() const override;
  std::string destination_active_user() const override;

 private:
  GURL source_url_;
  GURL destination_url_;
  raw_ptr<ProfileIOS> source_profile_;
  raw_ptr<ProfileIOS> destination_Profile_;
  ui::ClipboardMetadata metadata_;
};

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_UTILS_IOS_CLIPBOARD_CONTEXT_H_
