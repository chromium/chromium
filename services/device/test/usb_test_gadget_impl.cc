// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/stl_util.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "services/device/test/usb_test_gadget.h"
#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_device_handle.h"
#include "services/device/usb/usb_service.h"
#include "url/gurl.h"

namespace device {

class UsbTestGadgetImpl : public UsbTestGadget {
 public:
  UsbTestGadgetImpl(UsbService* usb_service, scoped_refptr<UsbDevice> device);

  UsbTestGadgetImpl(const UsbTestGadgetImpl&) = delete;
  UsbTestGadgetImpl& operator=(const UsbTestGadgetImpl&) = delete;

  ~UsbTestGadgetImpl() override;

  bool Unclaim() override;
  bool Disconnect() override;
  bool Reconnect() override;
  bool SetType(Type type) override;
  UsbDevice* GetDevice() const override;

 private:
  std::string device_address_;
  scoped_refptr<UsbDevice> device_;
  raw_ptr<UsbService> usb_service_;
};

namespace {

static const char kCommandLineSwitch[] = "enable-gadget-tests";
static const int kReenumeratePeriod = 100;  // 0.1 seconds

struct UsbTestGadgetConfiguration {
  UsbTestGadget::Type type;
  const char* http_resource;
  uint16_t product_id;
};

static const struct UsbTestGadgetConfiguration kConfigurations[] = {
    {UsbTestGadget::DEFAULT, "/unconfigure", 0x58F0},
    {UsbTestGadget::KEYBOARD, "/keyboard/configure", 0x58F1},
    {UsbTestGadget::MOUSE, "/mouse/configure", 0x58F2},
    {UsbTestGadget::HID_ECHO, "/hid_echo/configure", 0x58F3},
    {UsbTestGadget::ECHO, "/echo/configure", 0x58F4},
};

bool ReadFile(const base::FilePath& file_path, std::string* content) {
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(ERROR) << "Cannot open " << file_path.MaybeAsASCII() << ": "
               << base::File::ErrorToString(file.error_details());
    return false;
  }

  base::STLClearObject(content);
  int rv;
  do {
    char buf[4096];
    rv = file.ReadAtCurrentPos(buf, sizeof buf);
    if (rv == -1) {
      LOG(ERROR) << "Cannot read " << file_path.MaybeAsASCII() << ": "
                 << base::File::ErrorToString(file.error_details());
      return false;
    }
    content->append(buf, rv);
  } while (rv > 0);

  return true;
}

bool ReadLocalVersion(std::string* version) {
  base::FilePath file_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &file_path));
  file_path = file_path.AppendASCII("usb_gadget.zip.md5");

  return ReadFile(file_path, version);
}

bool ReadLocalPackage(std::string* package) {
  base::FilePath file_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &file_path));
  file_path = file_path.AppendASCII("usb_gadget.zip");

  return ReadFile(file_path, package);
}

class URLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  URLRequestContextGetter(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
      : network_task_runner_(network_task_runner) {}

 private:
  ~URLRequestContextGetter() override = default;

  // net::URLRequestContextGetter implementation
  net::URLRequestContext* GetURLRequestContext() override {
    if (!context_) {
      net::URLRequestContextBuilder context_builder;
      context_builder.set_proxy_resolution_service(
          net::ConfiguredProxyResolutionService::CreateDirect());
      context_ = context_builder.Build();
    }
    return context_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return network_task_runner_;
  }

  std::unique_ptr<net::URLRequestContext> context_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
};

std::unique_ptr<net::URLRequest> CreateSimpleRequest(
    net::URLRequestContext& request_context,
    net::URLRequest::Delegate* delegate,
    const GURL& url,
    const std::string& form_data_type,
    const std::string& form_data) {
  std::unique_ptr<net::URLRequest> request(request_context.CreateRequest(
      url, net::DEFAULT_PRIORITY, delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  if (!form_data_type.empty()) {
    net::HttpRequestHeaders extra_headers;
    extra_headers.SetHeader(net::HttpRequestHeaders::kContentType,
                            "application/x-www-form-urlencoded");
    request->SetExtraRequestHeaders(extra_headers);
  }
  if (!form_data.empty()) {
    auto reader = std::make_unique<net::UploadBytesElementReader>(
        base::as_byte_span(form_data));
    request->set_upload(
        net::ElementsUploadDataStream::CreateWithReader(std::move(reader)));
  }
  return request;
}

int SimplePOSTRequest(
    const GURL& url,
    const std::string& form_data) {
  net::TestDelegate delegate;
  auto request_context = net::CreateTestURLRequestContextBuilder()->Build();

  std::unique_ptr<net::URLRequest> request =
      CreateSimpleRequest(*request_context, &delegate, url,
                          "application/x-www-form-urlencoded", form_data);
  request->set_method("POST");

  request->Start();
  delegate.RunUntilComplete();

  return request->response_headers()->response_code();
}

class UsbGadgetFactory : public UsbService::Observer {
 public:
  // TODO(crbug.com/40101494): Remove `io_task_runner` parameter.
  UsbGadgetFactory(UsbService* usb_service,
                   scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
      : usb_service_(usb_service),
        request_context_(net::CreateTestURLRequestContextBuilder()->Build()) {
    // Gadget tests shouldn't be enabled without available |usb_service|.
    DCHECK(usb_service_);

    static uint32_t next_session_id;
    base::ProcessId process_id = base::GetCurrentProcId();
    session_id_ =
        base::StringPrintf("%" CrPRIdPid "-%d", process_id, next_session_id++);

    observation_.Observe(usb_service_.get());
  }

  ~UsbGadgetFactory() override = default;

  std::unique_ptr<UsbTestGadget> WaitForDevice() {
    EnumerateDevices();
    run_loop_.Run();
    return std::make_unique<UsbTestGadgetImpl>(usb_service_, device_);
  }

 private:
  void EnumerateDevices() {
    if (!device_) {
      usb_service_->GetDevices(base::BindOnce(
          &UsbGadgetFactory::OnDevicesEnumerated, weak_factory_.GetWeakPtr()));
    }
  }

  void OnDevicesEnumerated(
      const std::vector<scoped_refptr<UsbDevice>>& devices) {
    for (const scoped_refptr<UsbDevice>& device : devices) {
      OnDeviceAdded(device);
    }

    if (!device_) {
      // TODO(reillyg): This timer could be replaced by a way to use long-
      // polling to wait for claimed devices to become unclaimed.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&UsbGadgetFactory::EnumerateDevices,
                         weak_factory_.GetWeakPtr()),
          base::Milliseconds(kReenumeratePeriod));
    }
  }

  void OnDeviceAdded(scoped_refptr<UsbDevice> device) override {
    if (device_.get()) {
      // Already trying to claim a device.
      return;
    }

    if (device->vendor_id() != 0x18D1 || device->product_id() != 0x58F0 ||
        device->serial_number().empty()) {
      return;
    }

    std::string serial_number = base::UTF16ToUTF8(device->serial_number());
    if (serial_number == serial_number_) {
      // We were waiting for the device to reappear after upgrade.
      device_ = device;
      run_loop_.Quit();
      return;
    }

    device_ = device;
    serial_number_ = serial_number;
    Claim();
  }

  void Claim() {
    VLOG(1) << "Trying to claim " << serial_number_ << ".";

    GURL url("http://" + serial_number_ + "/claim");
    std::string form_data = base::StringPrintf(
        "session_id=%s", base::EscapeUrlEncodedData(session_id_, true).c_str());

    std::unique_ptr<net::URLRequest> request =
        CreateSimpleRequest(*request_context_, &delegate_, url,
                            "application/x-www-form-urlencoded", form_data);
    request->set_method("POST");
    request->Start();
    delegate_.set_on_complete(
        base::BindOnce(&UsbGadgetFactory::OnURLRequestComplete,
                       weak_factory_.GetWeakPtr(), std::move(request)));
  }

  void GetVersion() {
    GURL url("http://" + serial_number_ + "/version");

    std::unique_ptr<net::URLRequest> request = CreateSimpleRequest(
        *request_context_, &delegate_, url, std::string(), std::string());
    request->set_method("GET");
    request->Start();
    delegate_.set_on_complete(
        base::BindOnce(&UsbGadgetFactory::OnURLRequestComplete,
                       weak_factory_.GetWeakPtr(), std::move(request)));
  }

  bool Update(const std::string& version) {
    LOG(INFO) << "Updating " << serial_number_ << " to " << version << "...";

    GURL url("http://" + serial_number_ + "/update");

    std::string mime_header = base::StringPrintf(
        "--foo\r\n"
        "Content-Disposition: form-data; name=\"file\"; "
        "filename=\"usb_gadget-%s.zip\"\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n",
        version.c_str());
    std::string mime_footer("\r\n--foo--\r\n");

    std::string package;
    if (!ReadLocalPackage(&package)) {
      return false;
    }

    std::unique_ptr<net::URLRequest> request = CreateSimpleRequest(
        *request_context_, &delegate_, url, "multipart/form-data; boundary=foo",
        mime_header + package + mime_footer);
    request->set_method("POST");
    request->Start();
    delegate_.set_on_complete(
        base::BindOnce(&UsbGadgetFactory::OnURLRequestComplete,
                       weak_factory_.GetWeakPtr(), std::move(request)));

    device_ = nullptr;
    return true;
  }

  void OnURLRequestComplete(std::unique_ptr<net::URLRequest> request) {
    DCHECK(!serial_number_.empty());

    int response_code = request->response_headers()->response_code();
    if (!claimed_) {
      // Just completed a /claim request.
      if (response_code == 200) {
        claimed_ = true;
        GetVersion();
      } else {
        if (response_code != 403) {
          LOG(WARNING) << "Unexpected HTTP " << response_code
                       << " from /claim.";
        }
        Reset();
      }
    } else if (version_.empty()) {
      // Just completed a /version request.
      if (response_code != 200) {
        LOG(WARNING) << "Unexpected HTTP " << response_code
                     << " from /version.";
        Reset();
        return;
      }

      version_ = delegate_.data_received();
      if (version_.empty()) {
        LOG(WARNING) << "Failed to read body from /version.";
        Reset();
        return;
      }

      std::string local_version;
      if (!ReadLocalVersion(&local_version)) {
        Reset();
        return;
      }

      if (version_ == local_version) {
        run_loop_.Quit();
      } else {
        if (!Update(local_version)) {
          Reset();
        }
      }
    } else {
      // Just completed an /update request.
      if (response_code != 200) {
        LOG(WARNING) << "Unexpected HTTP " << response_code << " from /update.";
        Reset();
        return;
      }

      // Must wait for the device to reconnect.
    }
  }

  void Reset() {
    device_ = nullptr;
    serial_number_.clear();
    claimed_ = false;
    version_.clear();

    // Wait a bit and then try again to find an available device.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&UsbGadgetFactory::EnumerateDevices,
                       weak_factory_.GetWeakPtr()),
        base::Milliseconds(kReenumeratePeriod));
  }

  raw_ptr<UsbService> usb_service_ = nullptr;
  net::TestDelegate delegate_;
  std::unique_ptr<net::URLRequestContext> request_context_;
  std::string session_id_;
  scoped_refptr<UsbDevice> device_;
  std::string serial_number_;
  bool claimed_ = false;
  std::string version_;
  base::RunLoop run_loop_;
  base::ScopedObservation<UsbService, UsbService::Observer> observation_{this};
  base::WeakPtrFactory<UsbGadgetFactory> weak_factory_{this};
};

class DeviceAddListener : public UsbService::Observer {
 public:
  DeviceAddListener(UsbService* usb_service,
                    const std::string& serial_number,
                    int product_id)
      : usb_service_(usb_service),
        serial_number_(serial_number),
        product_id_(product_id) {
    observation_.Observe(usb_service_.get());
  }

  DeviceAddListener(const DeviceAddListener&) = delete;
  DeviceAddListener& operator=(const DeviceAddListener&) = delete;

  ~DeviceAddListener() override = default;

  scoped_refptr<UsbDevice> WaitForAdd() {
    usb_service_->GetDevices(base::BindOnce(
        &DeviceAddListener::OnDevicesEnumerated, weak_factory_.GetWeakPtr()));
    run_loop_.Run();
    return device_;
  }

 private:
  void OnDevicesEnumerated(
      const std::vector<scoped_refptr<UsbDevice>>& devices) {
    for (const scoped_refptr<UsbDevice>& device : devices) {
      OnDeviceAdded(device);
    }
  }

  void OnDeviceAdded(scoped_refptr<UsbDevice> device) override {
    if (device->vendor_id() == 0x18D1 && !device->serial_number().empty()) {
      const uint16_t product_id = device->product_id();
      if (product_id_ == -1) {
        bool found = false;
        for (size_t i = 0; i < std::size(kConfigurations); ++i) {
          if (product_id == kConfigurations[i].product_id) {
            found = true;
            break;
          }
        }
        if (!found) {
          return;
        }
      } else {
        if (product_id_ != product_id) {
          return;
        }
      }

      if (serial_number_ != base::UTF16ToUTF8(device->serial_number())) {
        return;
      }

      device_ = device;
      run_loop_.Quit();
    }
  }

  raw_ptr<UsbService> usb_service_;
  const std::string serial_number_;
  const int product_id_;
  base::RunLoop run_loop_;
  scoped_refptr<UsbDevice> device_;
  base::ScopedObservation<UsbService, UsbService::Observer> observation_{this};
  base::WeakPtrFactory<DeviceAddListener> weak_factory_{this};
};

class DeviceRemoveListener : public UsbService::Observer {
 public:
  DeviceRemoveListener(UsbService* usb_service, scoped_refptr<UsbDevice> device)
      : usb_service_(usb_service), device_(device) {
    observation_.Observe(usb_service_.get());
  }

  DeviceRemoveListener(const DeviceRemoveListener&) = delete;
  DeviceRemoveListener& operator=(const DeviceRemoveListener&) = delete;

  ~DeviceRemoveListener() override = default;

  void WaitForRemove() {
    usb_service_->GetDevices(
        base::BindOnce(&DeviceRemoveListener::OnDevicesEnumerated,
                       weak_factory_.GetWeakPtr()));
    run_loop_.Run();
  }

 private:
  void OnDevicesEnumerated(
      const std::vector<scoped_refptr<UsbDevice>>& devices) {
    bool found = false;
    for (const scoped_refptr<UsbDevice>& device : devices) {
      if (device_ == device) {
        found = true;
      }
    }
    if (!found) {
      run_loop_.Quit();
    }
  }

  void OnDeviceRemoved(scoped_refptr<UsbDevice> device) override {
    if (device_ == device) {
      run_loop_.Quit();
    }
  }

  raw_ptr<UsbService> usb_service_;
  base::RunLoop run_loop_;
  scoped_refptr<UsbDevice> device_;
  base::ScopedObservation<UsbService, UsbService::Observer> observation_{this};
  base::WeakPtrFactory<DeviceRemoveListener> weak_factory_{this};
};

}  // namespace

bool UsbTestGadget::IsTestEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kCommandLineSwitch);
}

std::unique_ptr<UsbTestGadget> UsbTestGadget::Claim(
    UsbService* usb_service,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  UsbGadgetFactory gadget_factory(usb_service, io_task_runner);
  return gadget_factory.WaitForDevice();
}

UsbTestGadgetImpl::UsbTestGadgetImpl(
    UsbService* usb_service,
    scoped_refptr<UsbDevice> device)
    : device_address_(base::UTF16ToUTF8(device->serial_number())),
      device_(device),
      usb_service_(usb_service) {}

UsbTestGadgetImpl::~UsbTestGadgetImpl() {
  if (!device_address_.empty()) {
    Unclaim();
  }
}

UsbDevice* UsbTestGadgetImpl::GetDevice() const {
  return device_.get();
}

bool UsbTestGadgetImpl::Unclaim() {
  VLOG(1) << "Releasing the device at " << device_address_ << ".";

  GURL url("http://" + device_address_ + "/unclaim");
  int response_code = SimplePOSTRequest(url, "");

  if (response_code != 200) {
    LOG(ERROR) << "Unexpected HTTP " << response_code << " from /unclaim.";
    return false;
  }

  device_address_.clear();
  return true;
}

bool UsbTestGadgetImpl::SetType(Type type) {
  const struct UsbTestGadgetConfiguration* config = NULL;
  for (size_t i = 0; i < std::size(kConfigurations); ++i) {
    if (kConfigurations[i].type == type) {
      config = &kConfigurations[i];
    }
  }
  CHECK(config);

  GURL url("http://" + device_address_ + config->http_resource);
  int response_code = SimplePOSTRequest(url, "");

  if (response_code != 200) {
    LOG(ERROR) << "Unexpected HTTP " << response_code << " from "
               << config->http_resource << ".";
    return false;
  }

  // Release the old reference to the device and try to open a new one.
  DeviceAddListener add_listener(usb_service_, device_address_,
                                 config->product_id);
  device_ = add_listener.WaitForAdd();
  DCHECK(device_.get());
  return true;
}

bool UsbTestGadgetImpl::Disconnect() {
  GURL url("http://" + device_address_ + "/disconnect");
  int response_code = SimplePOSTRequest(url, "");

  if (response_code != 200) {
    LOG(ERROR) << "Unexpected HTTP " << response_code << " from " << url << ".";
    return false;
  }

  // Release the old reference to the device and wait until it can't be found.
  DeviceRemoveListener remove_listener(usb_service_, device_);
  remove_listener.WaitForRemove();
  device_ = nullptr;
  return true;
}

bool UsbTestGadgetImpl::Reconnect() {
  GURL url("http://" + device_address_ + "/reconnect");
  int response_code = SimplePOSTRequest(url, "");

  if (response_code != 200) {
    LOG(ERROR) << "Unexpected HTTP " << response_code << " from " << url << ".";
    return false;
  }

  DeviceAddListener add_listener(usb_service_, device_address_, -1);
  device_ = add_listener.WaitForAdd();
  DCHECK(device_.get());
  return true;
}

}  // namespace device
