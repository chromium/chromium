// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_PEPPER_INTERFACE_URL_LOADER_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_PEPPER_INTERFACE_URL_LOADER_H_

#include <map>
#include <string>
#include <vector>

#include "fake_ppapi/fake_core_interface.h"
#include "fake_ppapi/fake_var_interface.h"
#include "fake_ppapi/fake_var_manager.h"
#include "nacl_io/pepper_interface_dummy.h"
#include "sdk_util/macros.h"

class FakeURLLoaderEntity {
 public:
  explicit FakeURLLoaderEntity(const std::string& body);
  FakeURLLoaderEntity(const std::string& to_repeat, off_t size);

  const std::string& body() const { return body_; }
  off_t size() { return size_; }

  size_t Read(void* buffer, size_t count, off_t offset);

 private:
  std::string body_;
  off_t size_;
  bool repeat_;
};

class FakeURLLoaderServer {
 public:
  FakeURLLoaderServer();

  void Clear();
  bool AddEntity(const std::string& url,
                 const std::string& body,
                 FakeURLLoaderEntity** out_entity);
  bool AddEntity(const std::string& url,
                 const std::string& body,
                 off_t size,
                 FakeURLLoaderEntity** out_entity);
  // Similar to AddEntity, but also allows partial requests and disallows HEAD
  // requests.
  bool SetBlobEntity(const std::string& url,
                     const std::string& body,
                     FakeURLLoaderEntity** out_entity);
  bool AddError(const std::string& url, int http_status_code);
  FakeURLLoaderEntity* GetEntity(const std::string& url);
  // Returns 0 if the url is not in the error map.
  int GetError(const std::string& url);

  // The maximum number of bytes that ReadResponseBody will send. If 0, then
  // send as many as are requested.
  void set_max_read_size(size_t max_read_size) {
    max_read_size_ = max_read_size;
  }

  // Whether to add the "Content-Length" header.
  void set_send_content_length(bool send_content_length) {
    send_content_length_ = send_content_length;
  }

  // Whether to allow partial reads (via the "Range" request header).
  void set_allow_partial(bool allow_partial) { allow_partial_ = allow_partial; }

  // Whether to allow HEAD requests.
  void set_allow_head(bool allow_head) { allow_head_ = allow_head; }

  size_t max_read_size() const { return max_read_size_; }
  bool send_content_length() const { return send_content_length_; }
  bool allow_partial() const { return allow_partial_; }
  bool allow_head() const { return allow_head_; }

 private:
  typedef std::map<std::string, FakeURLLoaderEntity> EntityMap;
  typedef std::map<std::string, int> ErrorMap;
  EntityMap entity_map_;
  ErrorMap error_map_;
  size_t max_read_size_;
  bool send_content_length_;
  bool allow_partial_;
  bool allow_head_;
};

class FakeURLLoaderInterface : public nacl_io::URLLoaderInterface {
 public:
  explicit FakeURLLoaderInterface(FakeCoreInterface* core_interface);

  FakeURLLoaderInterface(const FakeURLLoaderInterface&) = delete;
  FakeURLLoaderInterface& operator=(const FakeURLLoaderInterface&) = delete;

  virtual PP_Resource Create(PP_Instance instance);
  virtual int32_t Open(PP_Resource loader,
                       PP_Resource request_info,
                       PP_CompletionCallback callback);
  virtual PP_Resource GetResponseInfo(PP_Resource loader);
  virtual int32_t ReadResponseBody(PP_Resource loader,
                                   void* buffer,
                                   int32_t bytes_to_read,
                                   PP_CompletionCallback callback);
  virtual int32_t FinishStreamingToFile(PP_Resource loader,
                                        PP_CompletionCallback callback);

  virtual void Close(PP_Resource loader);

 protected:
  FakeCoreInterface* core_interface_;  // Weak reference.
};

class FakeURLRequestInfoInterface : public nacl_io::URLRequestInfoInterface {
 public:
  FakeURLRequestInfoInterface(FakeCoreInterface* core_interface,
                              FakeVarInterface* var_interface);

  FakeURLRequestInfoInterface(const FakeURLRequestInfoInterface&) = delete;
  FakeURLRequestInfoInterface& operator=(const FakeURLRequestInfoInterface&) =
      delete;

  virtual PP_Resource Create(PP_Instance instance);
  virtual PP_Bool SetProperty(PP_Resource request,
                              PP_URLRequestProperty property,
                              PP_Var value);
  virtual PP_Bool AppendDataToBody(PP_Resource request,
                                   const void* data,
                                   uint32_t len);

 protected:
  FakeCoreInterface* core_interface_;  // Weak reference.
  FakeVarInterface* var_interface_;    // Weak reference.
};

class FakeURLResponseInfoInterface : public nacl_io::URLResponseInfoInterface {
 public:
  FakeURLResponseInfoInterface(FakeCoreInterface* core_interface,
                               FakeVarInterface* var_interface);

  FakeURLResponseInfoInterface(const FakeURLResponseInfoInterface&) = delete;
  FakeURLResponseInfoInterface& operator=(const FakeURLResponseInfoInterface&) =
      delete;

  virtual PP_Var GetProperty(PP_Resource response,
                             PP_URLResponseProperty property);
  virtual PP_Resource GetBodyAsFileRef(PP_Resource response);

 protected:
  FakeCoreInterface* core_interface_;  // Weak reference.
  FakeVarInterface* var_interface_;    // Weak reference.
};

class FakePepperInterfaceURLLoader : public nacl_io::PepperInterfaceDummy {
 public:
  FakePepperInterfaceURLLoader();
  FakePepperInterfaceURLLoader(const FakeURLLoaderServer& server);

  FakePepperInterfaceURLLoader(const FakePepperInterfaceURLLoader&) = delete;
  FakePepperInterfaceURLLoader& operator=(const FakePepperInterfaceURLLoader&) =
      delete;

  ~FakePepperInterfaceURLLoader();

  virtual PP_Instance GetInstance() { return instance_; }
  virtual nacl_io::CoreInterface* GetCoreInterface();
  virtual nacl_io::VarInterface* GetVarInterface();
  virtual nacl_io::URLLoaderInterface* GetURLLoaderInterface();
  virtual nacl_io::URLRequestInfoInterface* GetURLRequestInfoInterface();
  virtual nacl_io::URLResponseInfoInterface* GetURLResponseInfoInterface();

  FakeURLLoaderServer* server_template() { return &server_template_; }

 private:
  void Init();

  FakeResourceManager resource_manager_;
  FakeCoreInterface core_interface_;
  FakeVarInterface var_interface_;
  FakeVarManager var_manager_;
  FakeURLLoaderServer server_template_;
  FakeURLLoaderInterface url_loader_interface_;
  FakeURLRequestInfoInterface url_request_info_interface_;
  FakeURLResponseInfoInterface url_response_info_interface_;
  PP_Instance instance_;
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_PEPPER_INTERFACE_URL_LOADER_H_
