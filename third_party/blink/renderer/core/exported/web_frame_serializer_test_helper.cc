// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/exported/web_frame_serializer_test_helper.h"

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_frame_serializer.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_archive.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_parser.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class SimpleMHTMLPartsGenerationDelegate
    : public WebFrameSerializer::MHTMLPartsGenerationDelegate {
 public:
  SimpleMHTMLPartsGenerationDelegate() : remove_popup_overlay_(false) {}

  void SetRemovePopupOverlay(bool remove_popup_overlay) {
    remove_popup_overlay_ = remove_popup_overlay;
  }

 private:
  bool ShouldSkipResource(const WebURL&) final { return false; }

  bool UseBinaryEncoding() final { return false; }
  bool RemovePopupOverlay() final { return remove_popup_overlay_; }

  bool remove_popup_overlay_;
};

String GenerateMHTMLHelper(WebLocalFrameImpl* frame,
                           const bool only_body_parts,
                           const bool remove_popup_overlay) {
  SimpleMHTMLPartsGenerationDelegate mhtml_delegate;
  mhtml_delegate.SetRemovePopupOverlay(remove_popup_overlay);

  // Boundaries are normally randomly generated but this one is predefined for
  // simplicity and as good as any other. Plus it gets used in almost all the
  // examples in the MHTML spec - RFC 2557.
  const WebString boundary("boundary-example");
  StringBuilder mhtml;
  if (!only_body_parts) {
    WebThreadSafeData header_result = WebFrameSerializer::GenerateMHTMLHeader(
        boundary, frame, &mhtml_delegate);
    mhtml.Append(header_result.data(),
                 static_cast<unsigned>(header_result.size()));
  }

  base::RunLoop run_loop;
  WebFrameSerializer::GenerateMHTMLParts(
      boundary, frame, &mhtml_delegate,
      WTF::BindOnce(
          [](StringBuilder* mhtml, base::OnceClosure quit,
             WebThreadSafeData data) {
            mhtml->Append(data.data(), static_cast<unsigned>(data.size()));
            std::move(quit).Run();
          },
          WTF::Unretained(&mhtml), run_loop.QuitClosure()));
  run_loop.Run();

  if (!only_body_parts) {
    scoped_refptr<RawData> footer_data = RawData::Create();
    MHTMLArchive::GenerateMHTMLFooterForTesting(boundary,
                                                *footer_data->MutableData());
    mhtml.Append(footer_data->data(),
                 static_cast<unsigned>(footer_data->size()));
  }

  String mhtml_string = mhtml.ToString();
  if (!only_body_parts) {
    // Validate the generated MHTML.
    MHTMLParser parser(SharedBuffer::Create(mhtml_string.Characters8(),
                                            size_t(mhtml_string.length())));
    EXPECT_FALSE(parser.ParseArchive().empty())
        << "Generated MHTML is not well formed";
  }
  return mhtml_string;
}

}  // namespace

String WebFrameSerializerTestHelper::GenerateMHTML(WebLocalFrameImpl* frame) {
  return GenerateMHTMLHelper(frame, false /*remove_popup_overlay*/,
                             false /*remove_popup_overlay*/);
}

String WebFrameSerializerTestHelper::GenerateMHTMLParts(
    WebLocalFrameImpl* frame) {
  return GenerateMHTMLHelper(frame, true /*remove_popup_overlay*/,
                             false /*remove_popup_overlay*/);
}

String WebFrameSerializerTestHelper::GenerateMHTMLWithPopupOverlayRemoved(
    WebLocalFrameImpl* frame) {
  return GenerateMHTMLHelper(frame, false /*remove_popup_overlay*/,
                             true /*remove_popup_overlay*/);
}

}  // namespace blink
