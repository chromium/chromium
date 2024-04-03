// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DATE_TIME_CHOOSER_DATE_TIME_CHOOSER_H_
#define CONTENT_BROWSER_DATE_TIME_CHOOSER_DATE_TIME_CHOOSER_H_

#include <string_view>

#include "content/common/content_export.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/choosers/date_time_chooser.mojom.h"

namespace content {
class WebContents;

class CONTENT_EXPORT DateTimeChooser
    : public blink::mojom::DateTimeChooser,
      public WebContentsUserData<DateTimeChooser> {
 public:
  static void CreateDateTimeChooser(WebContents* web_contents);
  static DateTimeChooser* GetDateTimeChooser(WebContents* web_contents);

  DateTimeChooser(const DateTimeChooser&) = delete;
  DateTimeChooser& operator=(const DateTimeChooser&) = delete;

  void OnDateTimeChooserReceiver(
      mojo::PendingReceiver<blink::mojom::DateTimeChooser> receiver);

  // blink::mojom::DateTimeChooser implementation:
  // Shows the dialog. |value| is the date/time value converted to a
  // number as defined in HTML. (See blink::InputType::parseToNumber())
  void OpenDateTimeDialog(blink::mojom::DateTimeDialogValuePtr value,
                          OpenDateTimeDialogCallback callback) final;

  void CloseDateTimeDialog() final;

 protected:
  explicit DateTimeChooser(WebContents* web_contents);
  ~DateTimeChooser() override;

  // Creates the platform specific dialog.
  virtual void OpenPlatformDialog(blink::mojom::DateTimeDialogValuePtr value,
                                  OpenDateTimeDialogCallback callback) {}
  // Closes the platform specific dialog opened by the method above.
  virtual void ClosePlatformDialog() {}

  void ReportBadMessage(std::string_view error);

  OpenDateTimeDialogCallback open_date_time_response_callback_;

 private:
  friend class content::WebContentsUserData<DateTimeChooser>;

  void OnDateTimeChooserReceiverConnectionError();

  mojo::Receiver<blink::mojom::DateTimeChooser> date_time_chooser_receiver_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_DATE_TIME_CHOOSER_DATE_TIME_CHOOSER_H_
