// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/date_time_chooser/android/date_time_chooser_android.h"

#include "base/run_loop.h"
#include "content/browser/date_time_chooser/date_time_chooser.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/mojom/choosers/date_time_chooser.mojom.h"
#include "ui/base/ime/mojom/ime_types.mojom.h"
#include "url/gurl.h"

namespace content {

class DateTimeChooserBrowserTest : public ContentBrowserTest {
 public:
  DateTimeChooserBrowserTest() = default;
  ~DateTimeChooserBrowserTest() override = default;

  WebContents* web_contents() const { return shell()->web_contents(); }

  blink::mojom::DateTimeDialogValuePtr CreateDummyDateTimeDialogValue() {
    auto date_time_dialog_value = blink::mojom::DateTimeDialogValue::New();
    date_time_dialog_value->dialog_type =
        ui::TextInputType::TEXT_INPUT_TYPE_MONTH;
    date_time_dialog_value->dialog_value = 0.0;
    date_time_dialog_value->minimum = 1.0;
    date_time_dialog_value->maximum = 1.0;
    date_time_dialog_value->step = 1.0;
    date_time_dialog_value->suggestions.push_back(
        blink::mojom::DateTimeSuggestion::New());
    return date_time_dialog_value;
  }

  void ResponseHandler(bool success, double dialog_value) {}
};

IN_PROC_BROWSER_TEST_F(DateTimeChooserBrowserTest,
                       ResetResponseCallbackViaDisconnectionHandler) {
  const GURL test_url(R"HTML(data:text/html,
      <input id=test type="date" list="src" />
        <datalist id="src">
          <option value='2022-01-29'/>
          <option value='2022-01-30'/>
        </datalist>
      )HTML");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  auto* date_time_chooser = static_cast<DateTimeChooserAndroid*>(
      DateTimeChooser::GetDateTimeChooser(web_contents()));
  ASSERT_TRUE(date_time_chooser);

  mojo::Remote<blink::mojom::DateTimeChooser> date_time_chooser_remote;
  date_time_chooser->OnDateTimeChooserReceiver(
      date_time_chooser_remote.BindNewPipeAndPassReceiver());

  auto response_callback = base::BindOnce(
      &DateTimeChooserBrowserTest::ResponseHandler, base::Unretained(this));

  // Open a date-time picker.
  date_time_chooser->OpenDateTimeDialog(CreateDummyDateTimeDialogValue(),
                                        std::move(response_callback));
  EXPECT_TRUE(date_time_chooser->j_date_time_chooser_);

  // Reset |date_time_chooser_remote| to call the disconnection error handler of
  // the date time chooser receiver in order to reset the previously registered
  // response callback.
  date_time_chooser_remote.reset();
  base::RunLoop().RunUntilIdle();

  // Check if the dialog UI is destroyed after the Mojo connection is
  // disconnected suddenly.
  EXPECT_FALSE(date_time_chooser->j_date_time_chooser_);

  // Open a date-time picker again on the same date time chooser and assertion
  // should not occur during OpenDateTimeDialog.
  date_time_chooser->OpenDateTimeDialog(CreateDummyDateTimeDialogValue(),
                                        std::move(response_callback));
  EXPECT_TRUE(date_time_chooser->j_date_time_chooser_);
}

}  // namespace content
