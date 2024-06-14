// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/dialogs/ui_bundled/overlay_java_script_dialog_presenter.h"

#import "base/functional/bind.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_alert_dialog_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_confirm_dialog_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_prompt_dialog_overlay.h"
#import "ios/chrome/browser/dialogs/ui_bundled/java_script_dialog_blocking_state.h"
#import "ios/web/public/web_state.h"

namespace {
// Completion callback for JavaScript alert dialog overlay.
void HandleJavaScriptAlertDialogResponse(
    base::OnceClosure callback,
    base::WeakPtr<web::WebState> weak_web_state,
    OverlayResponse* response) {
  // Notify the blocking state that the dialog was shown.
  web::WebState* web_state = weak_web_state.get();
  JavaScriptDialogBlockingState* blocking_state =
      web_state ? JavaScriptDialogBlockingState::FromWebState(web_state)
                : nullptr;
  if (blocking_state)
    blocking_state->JavaScriptDialogWasShown();

  JavaScriptAlertDialogResponse* dialog_response =
      response ? response->GetInfo<JavaScriptAlertDialogResponse>() : nullptr;
  if (!dialog_response) {
    // A null response is used if the dialog was not closed by user interaction.
    // This occurs either for navigation or because of WebState closures.
    std::move(callback).Run();
    return;
  }

  // Update the blocking state if the suppression action was selected.
  JavaScriptAlertDialogResponse::Action action = dialog_response->action();
  if (blocking_state &&
      action == JavaScriptAlertDialogResponse::Action::kBlockDialogs) {
    blocking_state->JavaScriptDialogBlockingOptionSelected();
  }

  std::move(callback).Run();
}

// Completion callback for JavaScript confirmation dialog overlay.
void HandleJavaScriptConfirmDialogResponse(
    base::OnceCallback<void(BOOL success)> callback,
    base::WeakPtr<web::WebState> weak_web_state,
    OverlayResponse* response) {
  // Notify the blocking state that the dialog was shown.
  web::WebState* web_state = weak_web_state.get();
  JavaScriptDialogBlockingState* blocking_state =
      web_state ? JavaScriptDialogBlockingState::FromWebState(web_state)
                : nullptr;
  if (blocking_state) {
    blocking_state->JavaScriptDialogWasShown();
  }

  JavaScriptConfirmDialogResponse* dialog_response =
      response ? response->GetInfo<JavaScriptConfirmDialogResponse>() : nullptr;
  if (!dialog_response) {
    // A null response is used if the dialog was not closed by user interaction.
    // This occurs either for navigation or because of WebState closures.
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // Update the blocking state if the suppression action was selected.
  JavaScriptConfirmDialogResponse::Action action = dialog_response->action();
  if (blocking_state &&
      action == JavaScriptConfirmDialogResponse::Action::kBlockDialogs) {
    blocking_state->JavaScriptDialogBlockingOptionSelected();
  }

  bool confirmed = action == JavaScriptConfirmDialogResponse::Action::kConfirm;
  std::move(callback).Run(confirmed);
}

// Completion callback for JavaScript prompt dialog overlay.
void HandleJavaScriptPromptDialogResponse(
    base::OnceCallback<void(NSString* user_input)> callback,
    base::WeakPtr<web::WebState> weak_web_state,
    OverlayResponse* response) {
  // Notify the blocking state that the dialog was shown.
  web::WebState* web_state = weak_web_state.get();
  JavaScriptDialogBlockingState* blocking_state =
      web_state ? JavaScriptDialogBlockingState::FromWebState(web_state)
                : nullptr;
  if (blocking_state) {
    blocking_state->JavaScriptDialogWasShown();
  }

  JavaScriptPromptDialogResponse* dialog_response =
      response ? response->GetInfo<JavaScriptPromptDialogResponse>() : nullptr;
  if (!dialog_response) {
    // A null response is used if the dialog was not closed by user interaction.
    // This occurs either for navigation or because of WebState closures.
    std::move(callback).Run(/*user_input=*/nil);
    return;
  }

  // Update the blocking state if the suppression action was selected.
  JavaScriptPromptDialogResponse::Action action = dialog_response->action();
  if (blocking_state &&
      action == JavaScriptPromptDialogResponse::Action::kBlockDialogs) {
    blocking_state->JavaScriptDialogBlockingOptionSelected();
  }

  NSString* user_input = nil;
  if (action == JavaScriptPromptDialogResponse::Action::kConfirm) {
    user_input = dialog_response->user_input();
  }
  std::move(callback).Run(user_input);
}
}  // namespace

OverlayJavaScriptDialogPresenter::OverlayJavaScriptDialogPresenter() = default;

OverlayJavaScriptDialogPresenter::OverlayJavaScriptDialogPresenter(
    OverlayJavaScriptDialogPresenter&& other) = default;

OverlayJavaScriptDialogPresenter& OverlayJavaScriptDialogPresenter::operator=(
    OverlayJavaScriptDialogPresenter&& other) = default;

OverlayJavaScriptDialogPresenter::~OverlayJavaScriptDialogPresenter() = default;

void OverlayJavaScriptDialogPresenter::RunJavaScriptAlertDialog(
    web::WebState* web_state,
    const GURL& origin_url,
    NSString* message_text,
    base::OnceClosure callback) {
  JavaScriptDialogBlockingState::CreateForWebState(web_state);
  if (JavaScriptDialogBlockingState::FromWebState(web_state)->blocked()) {
    // Block the dialog if needed.
    std::move(callback).Run();
    return;
  }

  bool from_main_frame_origin =
      origin_url.DeprecatedGetOriginAsURL() ==
      web_state->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<JavaScriptAlertDialogRequest>(
          web_state, origin_url, from_main_frame_origin, message_text);
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(&HandleJavaScriptAlertDialogResponse, std::move(callback),
                     web_state->GetWeakPtr()));
  OverlayRequestQueue::FromWebState(web_state, OverlayModality::kWebContentArea)
      ->AddRequest(std::move(request));
}

void OverlayJavaScriptDialogPresenter::RunJavaScriptConfirmDialog(
    web::WebState* web_state,
    const GURL& origin_url,
    NSString* message_text,
    base::OnceCallback<void(bool success)> callback) {
  JavaScriptDialogBlockingState::CreateForWebState(web_state);
  if (JavaScriptDialogBlockingState::FromWebState(web_state)->blocked()) {
    // Block the dialog if needed.
    std::move(callback).Run(/*success=*/false);
    return;
  }

  bool from_main_frame_origin =
      origin_url.DeprecatedGetOriginAsURL() ==
      web_state->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<JavaScriptConfirmDialogRequest>(
          web_state, origin_url, from_main_frame_origin, message_text);
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(&HandleJavaScriptConfirmDialogResponse,
                     std::move(callback), web_state->GetWeakPtr()));
  OverlayRequestQueue::FromWebState(web_state, OverlayModality::kWebContentArea)
      ->AddRequest(std::move(request));
}

void OverlayJavaScriptDialogPresenter::RunJavaScriptPromptDialog(
    web::WebState* web_state,
    const GURL& origin_url,
    NSString* message_text,
    NSString* default_prompt_text,
    base::OnceCallback<void(NSString* user_input)> callback) {
  JavaScriptDialogBlockingState::CreateForWebState(web_state);
  if (JavaScriptDialogBlockingState::FromWebState(web_state)->blocked()) {
    // Block the dialog if needed.
    std::move(callback).Run(/*user_input=*/nil);
    return;
  }

  bool from_main_frame_origin =
      origin_url.DeprecatedGetOriginAsURL() ==
      web_state->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<JavaScriptPromptDialogRequest>(
          web_state, origin_url, from_main_frame_origin, message_text,
          default_prompt_text);
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(&HandleJavaScriptPromptDialogResponse, std::move(callback),
                     web_state->GetWeakPtr()));
  OverlayRequestQueue::FromWebState(web_state, OverlayModality::kWebContentArea)
      ->AddRequest(std::move(request));
}

void OverlayJavaScriptDialogPresenter::CancelDialogs(web::WebState* web_state) {
  OverlayRequestQueue::FromWebState(web_state, OverlayModality::kWebContentArea)
      ->CancelAllRequests();
}
