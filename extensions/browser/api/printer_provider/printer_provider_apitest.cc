// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"
#include "extensions/browser/api/printer_provider/printer_provider_api_factory.h"
#include "extensions/browser/api/printer_provider/printer_provider_print_job.h"
#include "extensions/browser/api/usb/usb_device_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/value_builder.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Callback for PrinterProviderAPI::DispatchGetPrintersRequested calls.
// It appends items in |printers| to |*printers_out|. If |done| is set, it runs
// |callback|.
void AppendPrintersAndRunCallbackIfDone(base::ListValue* printers_out,
                                        const base::Closure& callback,
                                        const base::ListValue& printers,
                                        bool done) {
  for (size_t i = 0; i < printers.GetSize(); ++i) {
    const base::DictionaryValue* printer = NULL;
    EXPECT_TRUE(printers.GetDictionary(i, &printer))
        << "Found invalid printer value at index " << i << ": " << printers;
    if (printer)
      printers_out->Append(printer->CreateDeepCopy());
  }
  if (done && !callback.is_null())
    callback.Run();
}

// Callback for PrinterProviderAPI::DispatchPrintRequested calls.
// It fills the out params based on |status| and runs |callback|.
void RecordPrintResultAndRunCallback(bool* result_success,
                                     std::string* result_status,
                                     const base::Closure& callback,
                                     const base::Value& status) {
  bool success = status.is_none();
  std::string status_str = success ? "OK" : status.GetString();
  *result_success = success;
  *result_status = status_str;
  if (callback)
    callback.Run();
}

// Callback for PrinterProviderAPI::DispatchGetCapabilityRequested calls.
// It saves reported |value| as JSON string to |*result| and runs |callback|.
void RecordDictAndRunCallback(std::string* result,
                              const base::Closure& callback,
                              const base::DictionaryValue& value) {
  JSONStringValueSerializer serializer(result);
  EXPECT_TRUE(serializer.Serialize(value));
  if (!callback.is_null())
    callback.Run();
}

// Callback for PrinterProvider::DispatchGrantUsbPrinterAccess calls.
// It expects |value| to equal |expected_value| and runs |callback|.
void ExpectValueAndRunCallback(const base::Value* expected_value,
                               const base::Closure& callback,
                               const base::DictionaryValue& value) {
  EXPECT_TRUE(value.Equals(expected_value));
  if (!callback.is_null())
    callback.Run();
}

// Tests for chrome.printerProvider API.
class PrinterProviderApiTest : public ShellApiTest {
 public:
  enum PrintRequestDataType {
    PRINT_REQUEST_DATA_TYPE_NOT_SET,
    PRINT_REQUEST_DATA_TYPE_BYTES
  };

  PrinterProviderApiTest() {}
  ~PrinterProviderApiTest() override = default;

  void StartGetPrintersRequest(
      const PrinterProviderAPI::GetPrintersCallback& callback) {
    PrinterProviderAPIFactory::GetInstance()
        ->GetForBrowserContext(browser_context())
        ->DispatchGetPrintersRequested(callback);
  }

  void StartGetUsbPrinterInfoRequest(
      const std::string& extension_id,
      const device::mojom::UsbDeviceInfo& device,
      PrinterProviderAPI::GetPrinterInfoCallback callback) {
    PrinterProviderAPIFactory::GetInstance()
        ->GetForBrowserContext(browser_context())
        ->DispatchGetUsbPrinterInfoRequested(extension_id, device,
                                             std::move(callback));
  }

  void StartPrintRequestWithNoData(const std::string& extension_id,
                                   PrinterProviderAPI::PrintCallback callback) {
    PrinterProviderPrintJob job;
    job.printer_id = extension_id + ":printer_id";
    job.ticket = base::Value(base::Value::Type::DICTIONARY);
    job.content_type = "application/pdf";

    PrinterProviderAPIFactory::GetInstance()
        ->GetForBrowserContext(browser_context())
        ->DispatchPrintRequested(std::move(job), std::move(callback));
  }

  void StartPrintRequestUsingDocumentBytes(
      const std::string& extension_id,
      PrinterProviderAPI::PrintCallback callback) {
    PrinterProviderPrintJob job;
    job.printer_id = extension_id + ":printer_id";
    job.job_title = base::ASCIIToUTF16("Print job");
    job.ticket = base::Value(base::Value::Type::DICTIONARY);
    job.content_type = "application/pdf";
    const unsigned char kDocumentBytes[] = {'b', 'y', 't', 'e', 's'};
    job.document_bytes =
        new base::RefCountedBytes(kDocumentBytes, base::size(kDocumentBytes));

    PrinterProviderAPIFactory::GetInstance()
        ->GetForBrowserContext(browser_context())
        ->DispatchPrintRequested(std::move(job), std::move(callback));
  }

  void StartCapabilityRequest(
      const std::string& extension_id,
      PrinterProviderAPI::GetCapabilityCallback callback) {
    PrinterProviderAPIFactory::GetInstance()
        ->GetForBrowserContext(browser_context())
        ->DispatchGetCapabilityRequested(extension_id + ":printer_id",
                                         std::move(callback));
  }

  // Loads chrome.printerProvider test app and initializes is for test
  // |test_param|.
  // When the app's background page is loaded, the app will send 'loaded'
  // message. As a response to the message it will expect string message
  // specifying the test that should be run. When the app initializes its state
  // (e.g. registers listener for a chrome.printerProvider event) it will send
  // message 'ready', at which point the test may be started.
  // If the app is successfully initialized, |*extension_id_out| will be set to
  // the loaded extension's id, otherwise it will remain unchanged.
  void InitializePrinterProviderTestApp(const std::string& app_path,
                                        const std::string& test_param,
                                        std::string* extension_id_out) {
    ExtensionTestMessageListener loaded_listener("loaded", true);
    ExtensionTestMessageListener ready_listener("ready", false);

    const Extension* extension = LoadApp(app_path);
    ASSERT_TRUE(extension);
    const std::string extension_id = extension->id();

    loaded_listener.set_extension_id(extension_id);
    ready_listener.set_extension_id(extension_id);

    ASSERT_TRUE(loaded_listener.WaitUntilSatisfied());

    loaded_listener.Reply(test_param);

    ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

    *extension_id_out = extension_id;
  }

  // Runs a test for chrome.printerProvider.onPrintRequested event.
  // |test_param|: The test that should be run.
  // |expected_result|: The print result the app is expected to report.
  void RunPrintRequestTestApp(const std::string& test_param,
                              PrintRequestDataType data_type,
                              const std::string& expected_result) {
    ResultCatcher catcher;

    std::string extension_id;
    InitializePrinterProviderTestApp("api_test/printer_provider/request_print",
                                     test_param, &extension_id);
    if (extension_id.empty())
      return;

    base::RunLoop run_loop;
    bool success;
    std::string print_status;
    PrinterProviderAPI::PrintCallback callback =
        base::Bind(&RecordPrintResultAndRunCallback, &success, &print_status,
                   run_loop.QuitClosure());

    switch (data_type) {
      case PRINT_REQUEST_DATA_TYPE_NOT_SET:
        StartPrintRequestWithNoData(extension_id, std::move(callback));
        break;
      case PRINT_REQUEST_DATA_TYPE_BYTES:
        StartPrintRequestUsingDocumentBytes(extension_id, std::move(callback));
        break;
    }

    if (data_type != PRINT_REQUEST_DATA_TYPE_NOT_SET)
      ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

    run_loop.Run();
    EXPECT_EQ(expected_result, print_status);
    EXPECT_EQ(expected_result == "OK", success);
  }

  // Runs a test for chrome.printerProvider.onGetCapabilityRequested
  // event.
  // |test_param|: The test that should be run.
  // |expected_result|: The printer capability the app is expected to report.
  void RunPrinterCapabilitiesRequestTest(const std::string& test_param,
                                         const std::string& expected_result) {
    ResultCatcher catcher;

    std::string extension_id;
    InitializePrinterProviderTestApp(
        "api_test/printer_provider/request_capability", test_param,
        &extension_id);
    if (extension_id.empty())
      return;

    base::RunLoop run_loop;
    std::string result;
    StartCapabilityRequest(
        extension_id,
        base::Bind(&RecordDictAndRunCallback, &result, run_loop.QuitClosure()));

    ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

    run_loop.Run();
    EXPECT_EQ(expected_result, result);
  }

  // Run a test for the chrome.printerProvider.onGetUsbPrinterInfoRequested
  // event.
  // |test_param|: The test that should be run.
  // |expected_result|: The printer info that the app is expected to report.
  void RunUsbPrinterInfoRequestTest(const std::string& test_param) {
    ResultCatcher catcher;
    device::mojom::UsbDeviceInfoPtr device =
        usb_manager_.CreateAndAddDevice(0, 0, "Google", "USB Printer", "");

    std::string extension_id;
    InitializePrinterProviderTestApp("api_test/printer_provider/usb_printers",
                                     test_param, &extension_id);
    ASSERT_FALSE(extension_id.empty());

    std::unique_ptr<base::Value> expected_printer_info(
        new base::DictionaryValue());
    base::RunLoop run_loop;
    StartGetUsbPrinterInfoRequest(
        extension_id, *device,
        base::Bind(&ExpectValueAndRunCallback, expected_printer_info.get(),
                   run_loop.QuitClosure()));
    run_loop.Run();

    ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  bool SimulateExtensionUnload(const std::string& extension_id) {
    ExtensionRegistry* extension_registry =
        ExtensionRegistry::Get(browser_context());

    const Extension* extension = extension_registry->GetExtensionById(
        extension_id, ExtensionRegistry::ENABLED);
    if (!extension)
      return false;

    extension_registry->RemoveEnabled(extension_id);
    extension_registry->TriggerOnUnloaded(extension,
                                          UnloadedExtensionReason::TERMINATE);
    return true;
  }

  // Validates that set of printers reported by test apps via
  // chrome.printerProvider.onGetPritersRequested is the same as the set of
  // printers in |expected_printers|. |expected_printers| contains list of
  // printer objects formatted as a JSON string. It is assumed that the values
  // in |expoected_printers| are unique.
  void ValidatePrinterListValue(
      const base::ListValue& printers,
      const std::vector<std::unique_ptr<base::Value>>& expected_printers) {
    ASSERT_EQ(expected_printers.size(), printers.GetSize());
    for (const auto& printer_value : expected_printers) {
      EXPECT_TRUE(printers.Find(*printer_value) != printers.end())
          << "Unable to find " << *printer_value << " in " << printers;
    }
  }

 protected:
  device::FakeUsbDeviceManager usb_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrinterProviderApiTest);
};

// TODO(crbug.com/631983): Flaky on Linux and CrOS trybots.
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_PrintJobSuccess DISABLED_PrintJobSuccess
#else
#define MAYBE_PrintJobSuccess PrintJobSuccess
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)
IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, MAYBE_PrintJobSuccess) {
  RunPrintRequestTestApp("OK", PRINT_REQUEST_DATA_TYPE_BYTES, "OK");
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, PrintJobAsyncSuccess) {
  RunPrintRequestTestApp("ASYNC_RESPONSE", PRINT_REQUEST_DATA_TYPE_BYTES, "OK");
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, PrintJobFailed) {
  RunPrintRequestTestApp("INVALID_TICKET", PRINT_REQUEST_DATA_TYPE_BYTES,
                         "INVALID_TICKET");
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, NoPrintEventListener) {
  RunPrintRequestTestApp("NO_LISTENER", PRINT_REQUEST_DATA_TYPE_BYTES,
                         "FAILED");
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest,
                       PrintRequestInvalidCallbackParam) {
  RunPrintRequestTestApp("INVALID_VALUE", PRINT_REQUEST_DATA_TYPE_BYTES,
                         "FAILED");
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, PrintRequestDataNotSet) {
  RunPrintRequestTestApp("IGNORE_CALLBACK", PRINT_REQUEST_DATA_TYPE_NOT_SET,
                         "FAILED");
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, PrintRequestAppUnloaded) {
  ResultCatcher catcher;

  std::string extension_id;
  InitializePrinterProviderTestApp("api_test/printer_provider/request_print",
                                   "IGNORE_CALLBACK", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  base::RunLoop run_loop;
  bool success = false;
  std::string status;
  StartPrintRequestUsingDocumentBytes(
      extension_id, base::Bind(&RecordPrintResultAndRunCallback, &success,
                               &status, run_loop.QuitClosure()));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ASSERT_TRUE(SimulateExtensionUnload(extension_id));

  run_loop.Run();
  EXPECT_FALSE(success);
  EXPECT_EQ("FAILED", status);
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, GetCapabilitySuccess) {
  RunPrinterCapabilitiesRequestTest("OK", "{\"capability\":\"value\"}");
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, GetCapabilityAsyncSuccess) {
  RunPrinterCapabilitiesRequestTest("ASYNC_RESPONSE",
                                    "{\"capability\":\"value\"}");
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, EmptyCapability) {
  RunPrinterCapabilitiesRequestTest("EMPTY", "{}");
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, NoCapabilityEventListener) {
  RunPrinterCapabilitiesRequestTest("NO_LISTENER", "{}");
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, CapabilityInvalidValue) {
  RunPrinterCapabilitiesRequestTest("INVALID_VALUE", "{}");
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, GetCapabilityAppUnloaded) {
  ResultCatcher catcher;

  std::string extension_id;
  InitializePrinterProviderTestApp(
      "api_test/printer_provider/request_capability", "IGNORE_CALLBACK",
      &extension_id);
  ASSERT_FALSE(extension_id.empty());

  base::RunLoop run_loop;
  std::string result;
  StartCapabilityRequest(
      extension_id,
      base::Bind(&RecordDictAndRunCallback, &result, run_loop.QuitClosure()));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ASSERT_TRUE(SimulateExtensionUnload(extension_id));
  run_loop.Run();
  EXPECT_EQ("{}", result);
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, GetPrintersSuccess) {
  ResultCatcher catcher;

  std::string extension_id;
  InitializePrinterProviderTestApp("api_test/printer_provider/request_printers",
                                   "OK", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  base::RunLoop run_loop;
  base::ListValue printers;

  StartGetPrintersRequest(base::Bind(&AppendPrintersAndRunCallbackIfDone,
                                     &printers, run_loop.QuitClosure()));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  run_loop.Run();

  std::vector<std::unique_ptr<base::Value>> expected_printers;
  expected_printers.push_back(
      DictionaryBuilder()
          .Set("description", "Test printer")
          .Set("extensionId", extension_id)
          .Set("extensionName", "Test printer provider")
          .Set("id", base::StringPrintf("%s:printer1", extension_id.c_str()))
          .Set("name", "Printer 1")
          .Build());
  expected_printers.push_back(
      DictionaryBuilder()
          .Set("extensionId", extension_id)
          .Set("extensionName", "Test printer provider")
          .Set("id",
               base::StringPrintf("%s:printerNoDesc", extension_id.c_str()))
          .Set("name", "Printer 2")
          .Build());

  ValidatePrinterListValue(printers, expected_printers);
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, GetPrintersAsyncSuccess) {
  ResultCatcher catcher;

  std::string extension_id;
  InitializePrinterProviderTestApp("api_test/printer_provider/request_printers",
                                   "ASYNC_RESPONSE", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  base::RunLoop run_loop;
  base::ListValue printers;

  StartGetPrintersRequest(base::Bind(&AppendPrintersAndRunCallbackIfDone,
                                     &printers, run_loop.QuitClosure()));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  run_loop.Run();

  std::vector<std::unique_ptr<base::Value>> expected_printers;
  expected_printers.push_back(
      DictionaryBuilder()
          .Set("description", "Test printer")
          .Set("extensionId", extension_id)
          .Set("extensionName", "Test printer provider")
          .Set("id", base::StringPrintf("%s:printer1", extension_id.c_str()))
          .Set("name", "Printer 1")
          .Build());

  ValidatePrinterListValue(printers, expected_printers);
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, GetPrintersTwoExtensions) {
  ResultCatcher catcher;

  std::string extension_id_1;
  InitializePrinterProviderTestApp("api_test/printer_provider/request_printers",
                                   "OK", &extension_id_1);
  ASSERT_FALSE(extension_id_1.empty());

  std::string extension_id_2;
  InitializePrinterProviderTestApp(
      "api_test/printer_provider/request_printers_second", "OK",
      &extension_id_2);
  ASSERT_FALSE(extension_id_2.empty());

  base::RunLoop run_loop;
  base::ListValue printers;

  StartGetPrintersRequest(base::Bind(&AppendPrintersAndRunCallbackIfDone,
                                     &printers, run_loop.QuitClosure()));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  run_loop.Run();

  std::vector<std::unique_ptr<base::Value>> expected_printers;
  expected_printers.push_back(
      DictionaryBuilder()
          .Set("description", "Test printer")
          .Set("extensionId", extension_id_1)
          .Set("extensionName", "Test printer provider")
          .Set("id", base::StringPrintf("%s:printer1", extension_id_1.c_str()))
          .Set("name", "Printer 1")
          .Build());
  expected_printers.push_back(
      DictionaryBuilder()
          .Set("extensionId", extension_id_1)
          .Set("extensionName", "Test printer provider")
          .Set("id",
               base::StringPrintf("%s:printerNoDesc", extension_id_1.c_str()))
          .Set("name", "Printer 2")
          .Build());
  expected_printers.push_back(
      DictionaryBuilder()
          .Set("description", "Test printer")
          .Set("extensionId", extension_id_2)
          .Set("extensionName", "Test printer provider")
          .Set("id", base::StringPrintf("%s:printer1", extension_id_2.c_str()))
          .Set("name", "Printer 1")
          .Build());
  expected_printers.push_back(
      DictionaryBuilder()
          .Set("extensionId", extension_id_2)
          .Set("extensionName", "Test printer provider")
          .Set("id",
               base::StringPrintf("%s:printerNoDesc", extension_id_2.c_str()))
          .Set("name", "Printer 2")
          .Build());

  ValidatePrinterListValue(printers, expected_printers);
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest,
                       GetPrintersTwoExtensionsBothUnloaded) {
  ResultCatcher catcher;

  std::string extension_id_1;
  InitializePrinterProviderTestApp("api_test/printer_provider/request_printers",
                                   "IGNORE_CALLBACK", &extension_id_1);
  ASSERT_FALSE(extension_id_1.empty());

  std::string extension_id_2;
  InitializePrinterProviderTestApp(
      "api_test/printer_provider/request_printers_second", "IGNORE_CALLBACK",
      &extension_id_2);
  ASSERT_FALSE(extension_id_2.empty());

  base::RunLoop run_loop;
  base::ListValue printers;

  StartGetPrintersRequest(base::Bind(&AppendPrintersAndRunCallbackIfDone,
                                     &printers, run_loop.QuitClosure()));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ASSERT_TRUE(SimulateExtensionUnload(extension_id_1));
  ASSERT_TRUE(SimulateExtensionUnload(extension_id_2));

  run_loop.Run();

  EXPECT_TRUE(printers.empty());
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest,
                       GetPrintersTwoExtensionsOneFails) {
  ResultCatcher catcher;

  std::string extension_id_1;
  InitializePrinterProviderTestApp("api_test/printer_provider/request_printers",
                                   "NOT_ARRAY", &extension_id_1);
  ASSERT_FALSE(extension_id_1.empty());

  std::string extension_id_2;
  InitializePrinterProviderTestApp(
      "api_test/printer_provider/request_printers_second", "OK",
      &extension_id_2);
  ASSERT_FALSE(extension_id_2.empty());

  base::RunLoop run_loop;
  base::ListValue printers;

  StartGetPrintersRequest(base::Bind(&AppendPrintersAndRunCallbackIfDone,
                                     &printers, run_loop.QuitClosure()));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  run_loop.Run();

  std::vector<std::unique_ptr<base::Value>> expected_printers;
  expected_printers.push_back(
      DictionaryBuilder()
          .Set("description", "Test printer")
          .Set("extensionId", extension_id_2)
          .Set("extensionName", "Test printer provider")
          .Set("id", base::StringPrintf("%s:printer1", extension_id_2.c_str()))
          .Set("name", "Printer 1")
          .Build());
  expected_printers.push_back(
      DictionaryBuilder()
          .Set("extensionId", extension_id_2)
          .Set("extensionName", "Test printer provider")
          .Set("id",
               base::StringPrintf("%s:printerNoDesc", extension_id_2.c_str()))
          .Set("name", "Printer 2")
          .Build());

  ValidatePrinterListValue(printers, expected_printers);
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest,
                       GetPrintersTwoExtensionsOneWithNoListener) {
  ResultCatcher catcher;

  std::string extension_id_1;
  InitializePrinterProviderTestApp("api_test/printer_provider/request_printers",
                                   "NO_LISTENER", &extension_id_1);
  ASSERT_FALSE(extension_id_1.empty());

  std::string extension_id_2;
  InitializePrinterProviderTestApp(
      "api_test/printer_provider/request_printers_second", "OK",
      &extension_id_2);
  ASSERT_FALSE(extension_id_2.empty());

  base::RunLoop run_loop;
  base::ListValue printers;

  StartGetPrintersRequest(base::Bind(&AppendPrintersAndRunCallbackIfDone,
                                     &printers, run_loop.QuitClosure()));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  run_loop.Run();

  std::vector<std::unique_ptr<base::Value>> expected_printers;
  expected_printers.push_back(
      DictionaryBuilder()
          .Set("description", "Test printer")
          .Set("extensionId", extension_id_2)
          .Set("extensionName", "Test printer provider")
          .Set("id", base::StringPrintf("%s:printer1", extension_id_2.c_str()))
          .Set("name", "Printer 1")
          .Build());
  expected_printers.push_back(
      DictionaryBuilder()
          .Set("extensionId", extension_id_2)
          .Set("extensionName", "Test printer provider")
          .Set("id",
               base::StringPrintf("%s:printerNoDesc", extension_id_2.c_str()))
          .Set("name", "Printer 2")
          .Build());

  ValidatePrinterListValue(printers, expected_printers);
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, GetPrintersNoListener) {
  ResultCatcher catcher;

  std::string extension_id;
  InitializePrinterProviderTestApp("api_test/printer_provider/request_printers",
                                   "NO_LISTENER", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  base::RunLoop run_loop;
  base::ListValue printers;

  StartGetPrintersRequest(base::Bind(&AppendPrintersAndRunCallbackIfDone,
                                     &printers, run_loop.QuitClosure()));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  run_loop.Run();

  EXPECT_TRUE(printers.empty());
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, GetPrintersNotArray) {
  ResultCatcher catcher;

  std::string extension_id;
  InitializePrinterProviderTestApp("api_test/printer_provider/request_printers",
                                   "NOT_ARRAY", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  base::RunLoop run_loop;
  base::ListValue printers;

  StartGetPrintersRequest(base::Bind(&AppendPrintersAndRunCallbackIfDone,
                                     &printers, run_loop.QuitClosure()));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  run_loop.Run();

  EXPECT_TRUE(printers.empty());
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest,
                       GetPrintersInvalidPrinterValueType) {
  ResultCatcher catcher;

  std::string extension_id;
  InitializePrinterProviderTestApp("api_test/printer_provider/request_printers",
                                   "INVALID_PRINTER_TYPE", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  base::RunLoop run_loop;
  base::ListValue printers;

  StartGetPrintersRequest(base::Bind(&AppendPrintersAndRunCallbackIfDone,
                                     &printers, run_loop.QuitClosure()));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  run_loop.Run();

  EXPECT_TRUE(printers.empty());
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, GetPrintersInvalidPrinterValue) {
  ResultCatcher catcher;

  std::string extension_id;
  InitializePrinterProviderTestApp("api_test/printer_provider/request_printers",
                                   "INVALID_PRINTER", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  base::RunLoop run_loop;
  base::ListValue printers;

  StartGetPrintersRequest(base::Bind(&AppendPrintersAndRunCallbackIfDone,
                                     &printers, run_loop.QuitClosure()));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  run_loop.Run();

  EXPECT_TRUE(printers.empty());
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, GetUsbPrinterInfo) {
  ResultCatcher catcher;
  device::mojom::UsbDeviceInfoPtr device =
      usb_manager_.CreateAndAddDevice(0, 0, "Google", "USB Printer", "");

  std::string extension_id;
  InitializePrinterProviderTestApp("api_test/printer_provider/usb_printers",
                                   "OK", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  UsbDeviceManager* device_manager = UsbDeviceManager::Get(browser_context());
  std::unique_ptr<base::Value> expected_printer_info(
      DictionaryBuilder()
          .Set("description", "This printer is a USB device.")
          .Set("extensionId", extension_id)
          .Set("extensionName", "Test USB printer provider")
          .Set("id",
               base::StringPrintf("%s:usbDevice-%u", extension_id.c_str(),
                                  device_manager->GetIdFromGuid(device->guid)))
          .Set("name", "Test Printer")
          .Build());
  base::RunLoop run_loop;
  StartGetUsbPrinterInfoRequest(
      extension_id, *device,
      base::Bind(&ExpectValueAndRunCallback, expected_printer_info.get(),
                 run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, GetUsbPrinterInfoEmptyResponse) {
  RunUsbPrinterInfoRequestTest("EMPTY_RESPONSE");
}

IN_PROC_BROWSER_TEST_F(PrinterProviderApiTest, GetUsbPrinterInfoNoListener) {
  RunUsbPrinterInfoRequestTest("NO_LISTENER");
}

}  // namespace

}  // namespace extensions
