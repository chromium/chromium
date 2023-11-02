// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TRUETYPE_FONT_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TRUETYPE_FONT_HOST_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/renderer_host/pepper/pepper_truetype_font.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"

namespace content {

class BrowserPpapiHost;

class PepperTrueTypeFontHost : public ppapi::host::ResourceHost {
 public:
  PepperTrueTypeFontHost(BrowserPpapiHost* host,
                         PP_Instance instance,
                         PP_Resource resource,
                         const ppapi::proxy::SerializedTrueTypeFontDesc& desc);

  PepperTrueTypeFontHost(const PepperTrueTypeFontHost&) = delete;
  PepperTrueTypeFontHost& operator=(const PepperTrueTypeFontHost&) = delete;

  ~PepperTrueTypeFontHost() override;

  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

 private:
  int32_t OnHostMsgGetTableTags(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgGetTable(ppapi::host::HostMessageContext* context,
                            uint32_t table,
                            int32_t offset,
                            int32_t max_data_length);

  void OnInitializeComplete(ppapi::proxy::SerializedTrueTypeFontDesc* desc,
                            int32_t result);
  void OnGetTableTagsComplete(std::vector<uint32_t>* tags,
                              ppapi::host::ReplyMessageContext reply_context,
                              int32_t result);
  void OnGetTableComplete(std::string* data,
                          ppapi::host::ReplyMessageContext reply_context,
                          int32_t result);

  // We use a SequencedTaskRunner to run potentially slow font operations and
  // ensure that Initialize completes before we make any calls to get font data.
  // Even though we allow multiple pending GetTableTags and GetTable calls, this
  // implies that they run serially.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  scoped_refptr<PepperTrueTypeFont> font_;
  bool initialize_completed_;

  base::WeakPtrFactory<PepperTrueTypeFontHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TRUETYPE_FONT_HOST_H_
