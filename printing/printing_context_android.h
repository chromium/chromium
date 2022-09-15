// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_ANDROID_H_
#define PRINTING_PRINTING_CONTEXT_ANDROID_H_

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/file_descriptor_posix.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_context.h"

namespace ui {
class WindowAndroid;
}

namespace printing {

// Android subclass of PrintingContext. This class communicates with the
// Java side through JNI.
class COMPONENT_EXPORT(PRINTING) PrintingContextAndroid
    : public PrintingContext {
 public:
  explicit PrintingContextAndroid(Delegate* delegate);
  PrintingContextAndroid(const PrintingContextAndroid&) = delete;
  PrintingContextAndroid& operator=(const PrintingContextAndroid&) = delete;
  ~PrintingContextAndroid() override;

  // Called when the page is successfully written to a PDF using the file
  // descriptor specified, or when the printing operation failed. On success,
  // the PDF has `page_count` pages. Non-positive `page_count` indicates
  // failure.
  static void PdfWritingDone(int page_count);

  static void SetPendingPrint(
      ui::WindowAndroid* window,
      const base::android::ScopedJavaLocalRef<jobject>& printable,
      int render_process_id,
      int render_frame_id);

  // Called from Java, when printing settings from the user are ready or the
  // printing operation is canceled.
  void AskUserForSettingsReply(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               jboolean success);

  // Called from Java, when a printing process initiated by a script finishes.
  void ShowSystemDialogDone(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);

  // PrintingContext implementation.
  void AskUserForSettings(int max_pages,
                          bool has_selection,
                          bool is_scripted,
                          PrintSettingsCallback callback) override;
  mojom::ResultCode UseDefaultSettings() override;
  gfx::Size GetPdfPaperSizeDeviceUnits() override;
  mojom::ResultCode UpdatePrinterSettings(
      const PrinterSettings& printer_settings) override;
  mojom::ResultCode NewDocument(const std::u16string& document_name) override;
  mojom::ResultCode PrintDocument(const MetafilePlayer& metafile,
                                  const PrintSettings& settings,
                                  uint32_t num_pages) override;
  mojom::ResultCode DocumentDone() override;
  void Cancel() override;
  void ReleaseContext() override;
  printing::NativeDrawingContext context() const override;

 private:
  bool is_file_descriptor_valid() const { return fd_ > base::kInvalidFd; }

  base::android::ScopedJavaGlobalRef<jobject> j_printing_context_;

  // The callback from AskUserForSettings to be called when the settings are
  // ready on the Java side
  PrintSettingsCallback callback_;

  int fd_ = base::kInvalidFd;
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_ANDROID_H_
