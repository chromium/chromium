// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_selection/model/web_selection_tab_helper.h"

#import "ios/chrome/browser/web_selection/model/web_selection_java_script_feature.h"
#import "ios/chrome/browser/web_selection/model/web_selection_response.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"

namespace {

constexpr base::TimeDelta kSelectionRetrievalTimeout = base::Seconds(1);

void CallBothCallbacks(
    base::OnceCallback<void(WebSelectionResponse*)> callback1,
    base::OnceCallback<void(WebSelectionResponse*)> callback2,
    WebSelectionResponse* response) {
  std::move(callback1).Run(response);
  std::move(callback2).Run(response);
}

}  // namespace

WebSelectionTabHelper::WebSelectionTabHelper(web::WebState* web_state)
    : web_state_(web_state), weak_ptr_factory_(this) {
  web_state_->AddObserver(this);
}

WebSelectionTabHelper::~WebSelectionTabHelper() {}

void WebSelectionTabHelper::GetSelectedText(
    base::OnceCallback<void(WebSelectionResponse*)> callback) {
  DCHECK(callback);
  if (!web_state_) {
    std::move(callback).Run([WebSelectionResponse invalidResponse]);
    return;
  }
  if (!final_callback_) {
    if (WebSelectionJavaScriptFeature::GetInstance()->GetSelectedText(
            web_state_)) {
      WebSelectionJavaScriptFeature::GetInstance()->AddObserver(this);
      final_callback_ = std::move(callback);
      time_out_callback_.Start(FROM_HERE, kSelectionRetrievalTimeout,
                               base::BindOnce(&WebSelectionTabHelper::Timeout,
                                              weak_ptr_factory_.GetWeakPtr()));
    } else {
      std::move(callback).Run([WebSelectionResponse invalidResponse]);
      return;
    }
  } else {
    // If there is already a callback, then the selection is already being
    // retrieved. Just add the callback to the queue and continue the current
    // fetching.
    final_callback_ = BindOnce(CallBothCallbacks, std::move(final_callback_),
                               std::move(callback));
  }
}

void WebSelectionTabHelper::OnSelectionRetrieved(
    web::WebState* web_state,
    WebSelectionResponse* response) {
  if (web_state != web_state_) {
    // The selection comes from a different web_state. Ignore.
    return;
  }
  SendResponse(response);
}

bool WebSelectionTabHelper::CanRetrieveSelectedText() {
  if (!web_state_) {
    return false;
  }
  web::WebFrame* main_frame =
      web_state_->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (!web_state_->ContentIsHTML() || !main_frame) {
    return false;
  }
  return true;
}

void WebSelectionTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  SendResponse([WebSelectionResponse invalidResponse]);
  web_state_->RemoveObserver(this);
  web_state_ = nil;
}

void WebSelectionTabHelper::Timeout() {
  SendResponse([WebSelectionResponse invalidResponse]);
}

void WebSelectionTabHelper::SendResponse(WebSelectionResponse* response) {
  if (!final_callback_) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(final_callback_), response));
  WebSelectionJavaScriptFeature::GetInstance()->RemoveObserver(this);
  time_out_callback_.Stop();
}

WEB_STATE_USER_DATA_KEY_IMPL(WebSelectionTabHelper)
