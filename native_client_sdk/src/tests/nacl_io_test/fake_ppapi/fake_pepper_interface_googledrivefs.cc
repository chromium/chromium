// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_pepper_interface_googledrivefs.h"

#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_errors.h>

#include "gtest/gtest.h"

#include "fake_ppapi/fake_core_interface.h"
#include "fake_ppapi/fake_file_ref_interface.h"
#include "fake_ppapi/fake_pepper_interface_url_loader.h"
#include "fake_ppapi/fake_util.h"
#include "fake_ppapi/fake_var_interface.h"

class FakeDriveInstanceResource : public FakeResource {
 public:
  FakeDriveInstanceResource() : server_template(NULL) {}
  static const char* classname() { return "FakeDriveInstanceResource"; }

  FakeGoogleDriveServer* server_template;
};

class FakeDriveURLLoaderResource : public FakeResource {
 public:
  FakeDriveURLLoaderResource() : manager(NULL), server(NULL), response(0) {}

  virtual void Destroy() {
    EXPECT_TRUE(manager != NULL);
    if (response != 0)
      manager->Release(response);
  }

  static const char* classname() { return "FakeDriveURLLoaderResource"; }

  FakeResourceManager* manager;
  FakeGoogleDriveServer* server;
  PP_Resource response;
  std::string response_body;
};

class FakeDriveResponseResource : public FakeURLResponseInfoResource {
 public:
  FakeDriveResponseResource()
      : manager(NULL),
        loader_resource(NULL),
        file_ref(0),
        stream_to_file(false) {}

  virtual void Destroy() {
    EXPECT_TRUE(manager != NULL);
    if (file_ref != 0)
      manager->Release(file_ref);
  }

  static const char* classname() { return "FakeDriveResponseResource"; }

  FakeResourceManager* manager;
  FakeDriveURLLoaderResource* loader_resource;
  PP_Resource file_ref;
  bool stream_to_file;
  std::string body;
};

FakeGoogleDriveServer::FakeGoogleDriveServer() {}

void FakeGoogleDriveServer::Respond(
    const std::string& url,
    const std::string& headers,
    const std::string& method,
    const std::string& body,
    FakeGoogleDriveServerResponse* out_response) {
  out_response->status_code = STATUSCODE_NOT_IMPLEMENTED;
}

FakeDriveURLLoaderInterface::FakeDriveURLLoaderInterface(
    FakeCoreInterface* core_interface)
    : FakeURLLoaderInterface(core_interface) {}

PP_Resource FakeDriveURLLoaderInterface::Create(PP_Instance instance) {
  FakeDriveInstanceResource* drive_instance_resource =
      core_interface_->resource_manager()->Get<FakeDriveInstanceResource>(
          instance);
  if (drive_instance_resource == NULL)
    return 0;

  FakeDriveURLLoaderResource* drive_loader_resource =
      new FakeDriveURLLoaderResource;
  drive_loader_resource->manager = core_interface_->resource_manager();
  drive_loader_resource->server = drive_instance_resource->server_template;

  return CREATE_RESOURCE(core_interface_->resource_manager(),
                         FakeDriveURLLoaderResource, drive_loader_resource);
}

int32_t FakeDriveURLLoaderInterface::Open(PP_Resource loader,
                                          PP_Resource request,
                                          PP_CompletionCallback callback) {
  FakeDriveURLLoaderResource* drive_loader_resource =
      core_interface_->resource_manager()->Get<FakeDriveURLLoaderResource>(
          loader);
  if (drive_loader_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  // Close() has been called. Invalid to call Open() again.
  if (drive_loader_resource->server == NULL)
    return PP_ERROR_FAILED;

  FakeURLRequestInfoResource* request_resource =
      core_interface_->resource_manager()->Get<FakeURLRequestInfoResource>(
          request);
  if (request_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  // Create a response resource.
  FakeDriveResponseResource* drive_response_resource =
      new FakeDriveResponseResource;
  drive_response_resource->manager = drive_loader_resource->manager;
  drive_loader_resource->response =
      CREATE_RESOURCE(core_interface_->resource_manager(),
                      FakeDriveResponseResource, drive_response_resource);

  drive_response_resource->loader_resource = drive_loader_resource;
  drive_response_resource->stream_to_file = request_resource->stream_to_file;

  FakeGoogleDriveServerResponse server_response;
  drive_loader_resource->server->Respond(
      request_resource->url, request_resource->headers,
      request_resource->method, request_resource->body, &server_response);

  drive_response_resource->status_code = server_response.status_code;
  drive_loader_resource->response_body = server_response.body;

  // Call the callback.
  return RunCompletionCallback(&callback, PP_OK);
}

PP_Resource FakeDriveURLLoaderInterface::GetResponseInfo(PP_Resource loader) {
  FakeDriveURLLoaderResource* drive_loader_resource =
      core_interface_->resource_manager()->Get<FakeDriveURLLoaderResource>(
          loader);
  if (drive_loader_resource == NULL)
    return 0;

  if (drive_loader_resource->response == 0)
    return 0;

  // Returned resources have an implicit AddRef.
  core_interface_->resource_manager()->AddRef(drive_loader_resource->response);
  return drive_loader_resource->response;
}

int32_t FakeDriveURLLoaderInterface::FinishStreamingToFile(
    PP_Resource loader,
    PP_CompletionCallback callback) {
  FakeDriveURLLoaderResource* drive_loader_resource =
      core_interface_->resource_manager()->Get<FakeDriveURLLoaderResource>(
          loader);
  if (drive_loader_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  FakeDriveResponseResource* drive_response_resource =
      core_interface_->resource_manager()->Get<FakeDriveResponseResource>(
          drive_loader_resource->response);
  if (drive_response_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  if (!drive_response_resource->stream_to_file)
    return PP_ERROR_FAILED;

  drive_response_resource->body = drive_loader_resource->response_body;

  // PPB_URLLoader::FinishStreamingToFile(..) returns 0 even when
  // the response body size is > 0.
  return 0;
}

void FakeDriveURLLoaderInterface::Close(PP_Resource loader) {
  FakeDriveURLLoaderResource* drive_loader_resource =
      core_interface_->resource_manager()->Get<FakeDriveURLLoaderResource>(
          loader);
  if (drive_loader_resource == NULL)
    return;

  core_interface_->resource_manager()->Release(drive_loader_resource->response);

  drive_loader_resource->server = NULL;
  drive_loader_resource->response = 0;
  drive_loader_resource->response_body.clear();
}

FakeDriveURLRequestInfoInterface::FakeDriveURLRequestInfoInterface(
    FakeCoreInterface* core_interface,
    FakeVarInterface* var_interface)
    : FakeURLRequestInfoInterface(core_interface, var_interface) {}

PP_Resource FakeDriveURLRequestInfoInterface::Create(PP_Instance instance) {
  FakeDriveInstanceResource* drive_instance_resource =
      core_interface_->resource_manager()->Get<FakeDriveInstanceResource>(
          instance);
  if (drive_instance_resource == NULL)
    return PP_ERROR_BADRESOURCE;

  return CREATE_RESOURCE(core_interface_->resource_manager(),
                         FakeURLRequestInfoResource,
                         new FakeURLRequestInfoResource);
}

FakeDriveURLResponseInfoInterface::FakeDriveURLResponseInfoInterface(
    FakeCoreInterface* core_interface,
    FakeVarInterface* var_interface,
    FakeFileRefInterface* file_ref_interface)
    : FakeURLResponseInfoInterface(core_interface, var_interface),
      file_ref_interface_(file_ref_interface) {
  FakeFileSystemResource* file_system_resource = new FakeFileSystemResource;
  file_system_resource->filesystem = new FakeFilesystem();
  file_system_resource->opened = true;

  filesystem_resource_ =
      CREATE_RESOURCE(core_interface_->resource_manager(),
                      FakeFileSystemResource, file_system_resource);
}

FakeDriveURLResponseInfoInterface::~FakeDriveURLResponseInfoInterface() {
  core_interface_->ReleaseResource(filesystem_resource_);
}

PP_Var FakeDriveURLResponseInfoInterface::GetProperty(
    PP_Resource response,
    PP_URLResponseProperty property) {
  FakeDriveResponseResource* drive_response_resource =
      core_interface_->resource_manager()->Get<FakeDriveResponseResource>(
          response);
  if (drive_response_resource == NULL)
    return PP_Var();

  switch (property) {
    case PP_URLRESPONSEPROPERTY_URL:
      return var_interface_->VarFromUtf8(drive_response_resource->url.data(),
                                         drive_response_resource->url.size());

    case PP_URLRESPONSEPROPERTY_STATUSCODE:
      return PP_MakeInt32(drive_response_resource->status_code);

    case PP_URLRESPONSEPROPERTY_HEADERS:
      return var_interface_->VarFromUtf8(
          drive_response_resource->headers.data(),
          drive_response_resource->headers.size());
    default:
      EXPECT_TRUE(false) << "Unimplemented property " << property
                         << " in "
                            "FakeDriveURLResponseInfoInterface::GetProperty";
      return PP_Var();
  }
}

PP_Resource FakeDriveURLResponseInfoInterface::GetBodyAsFileRef(
    PP_Resource response) {
  FakeDriveResponseResource* drive_response_resource =
      core_interface_->resource_manager()->Get<FakeDriveResponseResource>(
          response);
  if (drive_response_resource == NULL)
    return 0;

  if (!drive_response_resource->stream_to_file)
    return 0;

  if (drive_response_resource->loader_resource == NULL)
    return 0;

  if (drive_response_resource->loader_resource->server == NULL)
    return 0;

  if (drive_response_resource->file_ref == 0) {
    FakeFileSystemResource* file_system_resource =
        core_interface_->resource_manager()->Get<FakeFileSystemResource>(
            filesystem_resource_);
    if (file_system_resource == NULL)
      return 0;

    char path_buffer[32];
    snprintf(path_buffer, sizeof(path_buffer), "%u", response);

    FakeNode* fake_node;
    if (!file_system_resource->filesystem->AddFile(
            path_buffer, drive_response_resource->body, &fake_node))
      return 0;

    drive_response_resource->file_ref =
        file_ref_interface_->Create(filesystem_resource_, path_buffer);
  }

  // Returned resources have an implicit AddRef.
  core_interface_->resource_manager()->AddRef(
      drive_response_resource->file_ref);
  return drive_response_resource->file_ref;
}

FakePepperInterfaceGoogleDriveFs::FakePepperInterfaceGoogleDriveFs()
    : core_interface_(&resource_manager_),
      var_interface_(&var_manager_),
      file_io_interface_(&core_interface_),
      file_ref_interface_(&core_interface_, &var_interface_),
      drive_url_loader_interface_(&core_interface_),
      drive_url_request_info_interface_(&core_interface_, &var_interface_),
      drive_url_response_info_interface_(&core_interface_,
                                         &var_interface_,
                                         &file_ref_interface_) {
  Init();
}

void FakePepperInterfaceGoogleDriveFs::Init() {
  FakeDriveInstanceResource* drive_instance_resource =
      new FakeDriveInstanceResource;
  drive_instance_resource->server_template = &google_drive_server_template_;

  instance_ =
      CREATE_RESOURCE(core_interface_.resource_manager(),
                      FakeDriveInstanceResource, drive_instance_resource);
}

FakePepperInterfaceGoogleDriveFs::~FakePepperInterfaceGoogleDriveFs() {
  core_interface_.ReleaseResource(instance_);
}

nacl_io::CoreInterface* FakePepperInterfaceGoogleDriveFs::GetCoreInterface() {
  return &core_interface_;
}

nacl_io::FileIoInterface*
FakePepperInterfaceGoogleDriveFs::GetFileIoInterface() {
  return &file_io_interface_;
}

nacl_io::FileRefInterface*
FakePepperInterfaceGoogleDriveFs::GetFileRefInterface() {
  return &file_ref_interface_;
}

nacl_io::URLLoaderInterface*
FakePepperInterfaceGoogleDriveFs::GetURLLoaderInterface() {
  return &drive_url_loader_interface_;
}

nacl_io::URLRequestInfoInterface*
FakePepperInterfaceGoogleDriveFs::GetURLRequestInfoInterface() {
  return &drive_url_request_info_interface_;
}

nacl_io::URLResponseInfoInterface*
FakePepperInterfaceGoogleDriveFs::GetURLResponseInfoInterface() {
  return &drive_url_response_info_interface_;
}

nacl_io::VarInterface* FakePepperInterfaceGoogleDriveFs::GetVarInterface() {
  return &var_interface_;
}
