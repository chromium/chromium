// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/message_box.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace remoting {

// MessageBox::Core creates the dialog using the views::DialogWidget.  The
// DialogWidget is created by the caller but its lifetime is managed by the
// NativeWidget.  The DialogWidget communicates with the caller using the
// DialogDelegateView interface, which must remain valid until DeleteDelegate()
// is called, at which the DialogDelegateView deletes itself.
//
// The Core class is introduced to abstract this awkward ownership model.  The
// Core and the MessageBox hold a raw references to each other, which is
// invalidated when either side are destroyed.
class MessageBox::Core : public views::DialogDelegateView {
  METADATA_HEADER(Core, views::DialogDelegateView)

 public:
  Core(const std::u16string& title_label,
       const std::u16string& message_label,
       const std::u16string& ok_label,
       const std::u16string& cancel_label,
       ResultCallback result_callback,
       MessageBox* message_box);
  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  // Mirrors the public MessageBox interface.
  void Show();
  void Hide();

  // views::DialogDelegateView:
  ui::mojom::ModalType GetModalType() const override;
  std::u16string GetWindowTitle() const override;
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // Called by MessageBox::Core when it is destroyed.
  void OnMessageBoxDestroyed();

 private:
  const std::u16string title_label_;
  ResultCallback result_callback_;
  raw_ptr<MessageBox> message_box_;

  // Owned by the native widget hierarchy.
  raw_ptr<views::MessageBoxView> message_box_view_;
};

MessageBox::Core::Core(const std::u16string& title_label,
                       const std::u16string& message_label,
                       const std::u16string& ok_label,
                       const std::u16string& cancel_label,
                       ResultCallback result_callback,
                       MessageBox* message_box)
    : title_label_(title_label),
      result_callback_(std::move(result_callback)),
      message_box_(message_box),
      message_box_view_(new views::MessageBoxView(message_label)) {
  DCHECK(message_box_);
  SetButtonLabel(ui::mojom::DialogButton::kOk, ok_label);
  SetButtonLabel(ui::mojom::DialogButton::kCancel, cancel_label);

  auto run_callback = [](MessageBox::Core* core, Result result) {
    if (core->result_callback_) {
      std::move(core->result_callback_).Run(result);
    }
  };
  SetAcceptCallback(base::BindOnce(run_callback, base::Unretained(this), OK));
  SetCancelCallback(
      base::BindOnce(run_callback, base::Unretained(this), CANCEL));
  SetCloseCallback(
      base::BindOnce(run_callback, base::Unretained(this), CANCEL));
  RegisterDeleteDelegateCallback(base::BindOnce(
      [](Core* dialog) {
        if (dialog->message_box_) {
          dialog->message_box_->core_ = nullptr;
        }
      },
      this));
}

void MessageBox::Core::Show() {
  // The widget is owned by the NativeWidget.  See  comments in widget.h.
  views::Widget* widget =
      CreateDialogWidget(this, /* delegate */
                         nullptr /* parent window*/, nullptr /* parent view */);

  if (widget) {
    widget->Show();
  }
}

void MessageBox::Core::Hide() {
  if (GetWidget()) {
    GetWidget()->Close();
  }
}

ui::mojom::ModalType MessageBox::Core::GetModalType() const {
  return ui::mojom::ModalType::kSystem;
}

std::u16string MessageBox::Core::GetWindowTitle() const {
  return title_label_;
}

views::View* MessageBox::Core::GetContentsView() {
  return message_box_view_;
}

views::Widget* MessageBox::Core::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* MessageBox::Core::GetWidget() const {
  return message_box_view_->GetWidget();
}

void MessageBox::Core::OnMessageBoxDestroyed() {
  DCHECK(message_box_);
  message_box_ = nullptr;
  // The callback should not be invoked after MessageBox is destroyed.
  result_callback_.Reset();
}

BEGIN_METADATA(MessageBox, Core)
END_METADATA

MessageBox::MessageBox(const std::u16string& title_label,
                       const std::u16string& message_label,
                       const std::u16string& ok_label,
                       const std::u16string& cancel_label,
                       ResultCallback result_callback)
    : core_(new Core(title_label,
                     message_label,
                     ok_label,
                     cancel_label,
                     std::move(result_callback),
                     this)) {}

void MessageBox::Show() {
  core_->Show();
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
