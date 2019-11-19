// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_win.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_hdc.h"
#include "printing/backend/printing_info_win.h"
#include "printing/backend/win_helper.h"
#include "printing/print_settings.h"
#include "printing/printing_context_system_dialog_win.h"
#include "printing/printing_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

// This test is automatically disabled if no printer is available.
class PrintingContextTest : public PrintingTest<testing::Test>,
                            public PrintingContext::Delegate {
 public:
  void PrintSettingsCallback(PrintingContext::Result result) {
    result_ = result;
  }

  // PrintingContext::Delegate methods.
  gfx::NativeView GetParentView() override { return nullptr; }
  std::string GetAppLocale() override { return std::string(); }

 protected:
  PrintingContext::Result result() const { return result_; }

 private:
  PrintingContext::Result result_;
};

namespace {

struct FreeHandleTraits {
  typedef HANDLE Handle;
  static void CloseHandle(HANDLE handle) { GlobalFree(handle); }
  static bool IsHandleValid(HANDLE handle) { return handle != nullptr; }
  static HANDLE NullHandle() { return nullptr; }
};

using ScopedGlobalAlloc =
    base::win::GenericScopedHandle<FreeHandleTraits,
                                   base::win::DummyVerifierTraits>;

}  // namespace

class MockPrintingContextWin : public PrintingContextSystemDialogWin {
 public:
  explicit MockPrintingContextWin(Delegate* delegate)
      : PrintingContextSystemDialogWin(delegate) {}

 protected:
  // This is a fake PrintDlgEx implementation that sets the right fields in
  // |lppd| to trigger a bug in older revisions of PrintingContext.
  HRESULT ShowPrintDialog(PRINTDLGEX* lppd) override {
    // The interesting bits:
    // Pretend the user hit print
    lppd->dwResultAction = PD_RESULT_PRINT;

    // Pretend the page range is 1-5, but since lppd->Flags does not have
    // PD_SELECTION set, this really shouldn't matter.
    lppd->nPageRanges = 1;
    lppd->lpPageRanges[0].nFromPage = 1;
    lppd->lpPageRanges[0].nToPage = 5;

    base::string16 printer_name = PrintingContextTest::GetDefaultPrinter();
    ScopedPrinterHandle printer;
    if (!printer.OpenPrinterWithName(printer_name.c_str()))
      return E_FAIL;

    const DEVMODE* dev_mode = nullptr;
    lppd->hDC = nullptr;
    lppd->hDevMode = nullptr;
    lppd->hDevNames = nullptr;

    PrinterInfo2 info_2;
    if (info_2.Init(printer.Get()))
      dev_mode = info_2.get()->pDevMode;
    if (!dev_mode)
      return E_FAIL;

    base::win::ScopedCreateDC hdc(
        CreateDC(L"WINSPOOL", printer_name.c_str(), nullptr, dev_mode));
    if (!hdc.Get())
      return E_FAIL;

    size_t dev_mode_size = dev_mode->dmSize + dev_mode->dmDriverExtra;
    ScopedGlobalAlloc dev_mode_mem(GlobalAlloc(GMEM_MOVEABLE, dev_mode_size));
    if (!dev_mode_mem.Get())
      return E_FAIL;
    void* dev_mode_ptr = GlobalLock(dev_mode_mem.Get());
    if (!dev_mode_ptr)
      return E_FAIL;
    memcpy(dev_mode_ptr, dev_mode, dev_mode_size);
    GlobalUnlock(dev_mode_mem.Get());
    dev_mode_ptr = nullptr;

    size_t driver_size =
        2 + sizeof(wchar_t) * lstrlen(info_2.get()->pDriverName);
    size_t printer_size =
        2 + sizeof(wchar_t) * lstrlen(info_2.get()->pPrinterName);
    size_t port_size = 2 + sizeof(wchar_t) * lstrlen(info_2.get()->pPortName);
    size_t dev_names_size =
        sizeof(DEVNAMES) + driver_size + printer_size + port_size;
    ScopedGlobalAlloc dev_names_mem(GlobalAlloc(GHND, dev_names_size));
    if (!dev_names_mem.Get())
      return E_FAIL;
    void* dev_names_ptr = GlobalLock(dev_names_mem.Get());
    if (!dev_names_ptr)
      return E_FAIL;
    DEVNAMES* dev_names = reinterpret_cast<DEVNAMES*>(dev_names_ptr);
    dev_names->wDefault = 1;
    dev_names->wDriverOffset = sizeof(DEVNAMES) / sizeof(wchar_t);
    memcpy(reinterpret_cast<uint8_t*>(dev_names_ptr) + dev_names->wDriverOffset,
           info_2.get()->pDriverName, driver_size);
    dev_names->wDeviceOffset = base::checked_cast<WORD>(
        dev_names->wDriverOffset + driver_size / sizeof(wchar_t));
    memcpy(reinterpret_cast<uint8_t*>(dev_names_ptr) + dev_names->wDeviceOffset,
           info_2.get()->pPrinterName, printer_size);
    dev_names->wOutputOffset = base::checked_cast<WORD>(
        dev_names->wDeviceOffset + printer_size / sizeof(wchar_t));
    memcpy(reinterpret_cast<uint8_t*>(dev_names_ptr) + dev_names->wOutputOffset,
           info_2.get()->pPortName, port_size);
    GlobalUnlock(dev_names_mem.Get());
    dev_names_ptr = nullptr;

    lppd->hDC = hdc.Take();
    lppd->hDevMode = dev_mode_mem.Take();
    lppd->hDevNames = dev_names_mem.Take();
    return S_OK;
  }
};

TEST_F(PrintingContextTest, PrintAll) {
  if (IsTestCaseDisabled())
    return;

  base::test::SingleThreadTaskEnvironment task_environment;
  MockPrintingContextWin context(this);
  context.AskUserForSettings(
      123, false, false,
      base::BindOnce(&PrintingContextTest::PrintSettingsCallback,
                     base::Unretained(this)));
  EXPECT_EQ(PrintingContext::OK, result());
  const PrintSettings& settings = context.settings();
  EXPECT_EQ(0u, settings.ranges().size());
}

TEST_F(PrintingContextTest, Color) {
  if (IsTestCaseDisabled())
    return;

  base::test::SingleThreadTaskEnvironment task_environment;
  MockPrintingContextWin context(this);
  context.AskUserForSettings(
      123, false, false,
      base::BindOnce(&PrintingContextTest::PrintSettingsCallback,
                     base::Unretained(this)));
  EXPECT_EQ(PrintingContext::OK, result());
  const PrintSettings& settings = context.settings();
  EXPECT_NE(settings.color(), UNKNOWN_COLOR_MODEL);
}

TEST_F(PrintingContextTest, Base) {
  if (IsTestCaseDisabled())
    return;

  auto settings = std::make_unique<PrintSettings>();
  settings->set_device_name(GetDefaultPrinter());
  // Initialize it.
  PrintingContextWin context(this);
  EXPECT_EQ(PrintingContext::OK,
            context.InitWithSettingsForTest(std::move(settings)));

  // The print may lie to use and may not support world transformation.
  // Verify right now.
  XFORM random_matrix = {1, 0.1f, 0, 1.5f, 0, 1};
  EXPECT_TRUE(SetWorldTransform(context.context(), &random_matrix));
  EXPECT_TRUE(ModifyWorldTransform(context.context(), nullptr, MWT_IDENTITY));
}

}  // namespace printing
