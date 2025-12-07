// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/message_box.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace remoting {

// MessageBoxCore creates the dialog using the views::DialogWidget.  The
// DialogWidget is created by the caller but its lifetime is managed by the
// NativeWidget.  The DialogWidget communicates with the caller using the
// DialogDelegateView interface, which must remain valid until DeleteDelegate()
// is called, at which the DialogDelegateView deletes itself.
//
// The MessageBoxCore class is introduced to abstract this awkward ownership
// model.  The MessageBoxCore and the MessageBox hold raw references to each
// other, which are invalidated when either side is destroyed.
class MessageBoxCore : public views::DialogDelegateView {
  METADATA_HEADER(MessageBoxCore, views::DialogDelegateView)

 public:
  MessageBoxCore(const std::u16string& title_label,
                 const std::u16string& message_label,
                 const std::u16string& ok_label,
                 const std::u16string& cancel_label,
                 const std::optional<ui::ImageModel> icon,
                 MessageBox::ResultCallback result_callback,
                 MessageBox* message_box);
  MessageBoxCore(const MessageBoxCore&) = delete;
  MessageBoxCore& operator=(const MessageBoxCore&) = delete;
  ~MessageBoxCore() override;

  void Show(gfx::NativeView parent);
  void Hide();

  void ChangeParentContainer(gfx::NativeView container);

  // views::DialogDelegateView:
  ui::mojom::ModalType GetModalType() const override;
  std::u16string GetWindowTitle() const override;
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  void SetMessageLabel(const std::u16string& message_label);

  // Called by MessageBox when it is destroyed.
  void OnMessageBoxDestroyed();

 private:
  const std::u16string title_label_;
  MessageBox::ResultCallback result_callback_;
  raw_ptr<MessageBox> message_box_;

  bool is_shown_ = false;

  // Owned by the native widget hierarchy.
  raw_ptr<views::MessageBoxView> message_box_view_;
};

MessageBoxCore::MessageBoxCore(const std::u16string& title_label,
                               const std::u16string& message_label,
                               const std::u16string& ok_label,
                               const std::u16string& cancel_label,
                               const std::optional<ui::ImageModel> icon,
                               MessageBox::ResultCallback result_callback,
                               MessageBox* message_box)
    : title_label_(title_label),
      result_callback_(std::move(result_callback)),
      message_box_(message_box),
      message_box_view_(new views::MessageBoxView(message_label)) {
  DCHECK(message_box_);
  SetButtonLabel(ui::mojom::DialogButton::kOk, ok_label);
  SetButtonLabel(ui::mojom::DialogButton::kCancel, cancel_label);

  if (icon.has_value()) {
    SetIcon(*icon);
    SetShowIcon(true);
  }

  auto run_callback = [](MessageBoxCore* core, MessageBox::Result result) {
    if (core->result_callback_) {
      std::move(core->result_callback_).Run(result);
    }
  };
  SetAcceptCallback(
      base::BindOnce(run_callback, base::Unretained(this), MessageBox::OK));
  SetCancelCallback(
      base::BindOnce(run_callback, base::Unretained(this), MessageBox::CANCEL));
  SetCloseCallback(
      base::BindOnce(run_callback, base::Unretained(this), MessageBox::CANCEL));
  RegisterDeleteDelegateCallback(
      RegisterDeleteCallbackPassKey(),
      base::BindOnce(
          [](MessageBoxCore* dialog) {
            if (dialog->message_box_) {
              dialog->message_box_->OnMessageBoxCoreDestroying();
            }
          },
          this));

  // This should be set as the `message_box_view_` is assumed to be owned by the
  // widget created.
  SetOwnedByWidget(OwnedByWidgetPassKey());
}

MessageBoxCore::~MessageBoxCore() = default;

void MessageBoxCore::Show(gfx::NativeView parent) {
  CHECK(!is_shown_) << "Show() should only be called once.";
  is_shown_ = true;

  // The widget is owned by the NativeWidget.  See  comments in widget.h.
  views::Widget* widget =
      CreateDialogWidget(/* delegate=*/this,
                         /*context=*/nullptr, /*parent=*/parent);

  if (widget) {
    widget->Show();
  }
}

void MessageBoxCore::ChangeParentContainer(gfx::NativeView parent) {
  if (GetWidget()) {
    views::Widget::ReparentNativeView(GetWidget()->GetNativeView(),
                                      /*new_parent=*/parent);
  }
}

void MessageBoxCore::Hide() {
  if (GetWidget()) {
    GetWidget()->Close();
  }
}

ui::mojom::ModalType MessageBoxCore::GetModalType() const {
  return ui::mojom::ModalType::kSystem;
}

std::u16string MessageBoxCore::GetWindowTitle() const {
  return title_label_;
}

views::View* MessageBoxCore::GetContentsView() {
  return message_box_view_;
}

views::Widget* MessageBoxCore::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* MessageBoxCore::GetWidget() const {
  return message_box_view_->GetWidget();
}

void MessageBoxCore::SetMessageLabel(const std::u16string& message_label) {
  message_box_view_->SetMessageLabel(message_label);
}

void MessageBoxCore::OnMessageBoxDestroyed() {
  DCHECK(message_box_);
  message_box_ = nullptr;
  // The callback should not be invoked after MessageBox is destroyed.
  result_callback_.Reset();
}

BEGIN_METADATA(MessageBoxCore)
END_METADATA

MessageBox::MessageBox(const std::u16string& title_label,
                       const std::u16string& message_label,
                       const std::u16string& ok_label,
                       const std::u16string& cancel_label,
                       const std::optional<ui::ImageModel> icon,
                       ResultCallback result_callback)
    : core_(new MessageBoxCore(title_label,
                               message_label,
                               ok_label,
                               cancel_label,
                               std::move(icon),
                               std::move(result_callback),
                               this)) {}

void MessageBox::Show() {
  core_->Show(nullptr);
}

void MessageBox::ShowInParentContainer(gfx::NativeView parent) {
  core_->Show(parent);
}

void MessageBox::ChangeParentContainer(gfx::NativeView parent) {
  core_->ChangeParentContainer(parent);
}

void MessageBox::SetMessageLabel(const std::u16string& message_label) {
  core_->SetMessageLabel(message_label);
}

views::DialogDelegate& MessageBox::GetDialogDelegate() {
  return CHECK_DEREF(core_->AsDialogDelegate());
}

void MessageBox::OnMessageBoxCoreDestroying() {
  core_ = nullptr;
}

MessageBox::~MessageBox() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (core_) {
    core_->OnMessageBoxDestroyed();
    core_->Hide();
    core_ = nullptr;
  }
}

}  // namespace remoting
