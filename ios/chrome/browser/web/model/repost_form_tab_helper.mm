// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/repost_form_tab_helper.h"

#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/web/model/repost_form_tab_helper_delegate.h"

namespace {

// Helper that returns a callback calling `closure`, and then `callback` with
// all parameters passed to the callback.
//
// This could look similar to base::OnceCallback<...>::Then(...) except that
// the callback returned by `Then()` invokes the original callback with the
// arguments, and then invoke a closure. Here, we call the closure first and
// then invoke the wrapped callback with the arguments passed to the created
// callback.
template <typename R, typename... Args>
base::OnceCallback<R(Args...)> ClosureBeforeCallback(
    base::OnceClosure closure,
    base::OnceCallback<R(Args...)> callback) {
  return base::BindOnce(
      [](base::OnceClosure inner_closure,
         base::OnceCallback<R(Args...)> inner_callback, Args... args) -> R {
        std::move(inner_closure).Run();
        return std::move(inner_callback).Run(std::forward<Args>(args)...);
      },
      std::move(closure), std::move(callback));
}

}  // namespace

RepostFormTabHelper::RepostFormTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

RepostFormTabHelper::~RepostFormTabHelper() {
  DCHECK(!web_state_);
}

void RepostFormTabHelper::DismissReportFormDialog() {
  weak_factory_.InvalidateWeakPtrs();
  if (is_presenting_dialog_)
    [delegate_ repostFormTabHelperDismissRepostFormDialog:this];
  is_presenting_dialog_ = false;
}

void RepostFormTabHelper::OnDialogPresented() {
  DCHECK(is_presenting_dialog_);
  is_presenting_dialog_ = false;
}

void RepostFormTabHelper::PresentDialog(
    CGPoint location,
    base::OnceCallback<void(bool)> callback) {
  // As seen in crbug.com/327948110, it is possible under unknwon circumstances
  // for ReportFormTabHelper::PresentDialog(...) to be called while a dialog is
  // presented. In that case, dismiss any previous dialog.
  if (is_presenting_dialog_) {
    DismissReportFormDialog();
  }

  if (!delegate_) {
    // If there is is no delegate, then assume that we should not continue.
    std::move(callback).Run(/*should_continue*/ false);
    return;
  }

  is_presenting_dialog_ = true;
  base::OnceClosure on_dialog_presented = base::BindOnce(
      &RepostFormTabHelper::OnDialogPresented, weak_factory_.GetWeakPtr());

  [delegate_ repostFormTabHelper:this
      presentRepostFormDialogForWebState:web_state_
                           dialogAtPoint:location
                       completionHandler:base::CallbackToBlock(
                                             ClosureBeforeCallback(
                                                 std::move(on_dialog_presented),
                                                 std::move(callback)))];
}

void RepostFormTabHelper::SetDelegate(
    id<RepostFormTabHelperDelegate> delegate) {
  delegate_ = delegate;
}

void RepostFormTabHelper::WasHidden(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  DismissReportFormDialog();
}

void RepostFormTabHelper::DidStartNavigation(web::WebState* web_state,
                                             web::NavigationContext*) {
  DCHECK_EQ(web_state_, web_state);
  DismissReportFormDialog();
}

void RepostFormTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  DismissReportFormDialog();

  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(RepostFormTabHelper)
