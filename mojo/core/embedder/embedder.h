// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_EMBEDDER_EMBEDDER_H_
#define MOJO_CORE_EMBEDDER_EMBEDDER_H_

#include <stddef.h>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core {

// Basic configuration/initialization ------------------------------------------

// Must be called first, or just after setting configuration parameters, to
// initialize the (global, singleton) system state. There is no corresponding
// shutdown operation: once the embedder is initialized, public Mojo C API calls
// remain available for the remainder of the process's lifetime.
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER)
void Init(const Configuration& configuration);

// Like above but uses a default Configuration.
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER) void Init();

// Explicitly shuts down Mojo stopping any IO thread work and destroying any
// global state initialized by Init().
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER) void ShutDown();

// Initialialization/shutdown for interprocess communication (IPC) -------------

// Retrieves the SequencedTaskRunner used for IPC I/O, as set by
// ScopedIPCSupport.
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER)
scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner();

// InitFeatures will be called as soon as the base::FeatureList is initialized.
// NOTE: This is temporarily necessary because of how Mojo is started with
// respect to base::FeatureList.
//
// TODO(rockot): Remove once a long term solution is in place for using
// base::Features inside of Mojo.
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER) void InitFeatures();

// Enables MojoIpcz. Called before Init() is called. Only call this if the
// current program doesn't have base::FeatureList integration, since otherwise
// InitFeatures() will do the work.
//
// TODO(crbug.com/40058840): Remove once MojoIpcz becomes the default
// implementation.
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER) void EnableMojoIpcz();

// Indicates whether the ipcz-based Mojo implementation is enabled. This can be
// done by enabling the MojoIpcz feature.
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER) bool IsMojoIpczEnabled();

// Installs base shared shared memory allocation hooks appropriate for use in
// a sandboxed environment when MojoIpcz is enabled on platforms where such
// processes cannot allocate shared memory directly through the OS. Must be
// called before any shared memory allocation is attempted in the process.
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER)
void InstallMojoIpczBaseSharedMemoryHooks();

// These functions expose the IpczAPI and IpczDriver structures used internally
// by the Mojo Core implementation when MojoIpcz is enabled.
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER) const IpczAPI& GetIpczAPIForMojo();
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER) const IpczDriver& GetIpczDriverForMojo();

// Creates a new ipcz driver transport from `endpoint`. The caller must specify
// whether each end of the transport will be attached to a broker node. If the
// local endpoint is a broker, `process` identifies the remote process if
// possible.
struct TransportEndpointTypes {
  bool local_is_broker;
  bool remote_is_broker;
};
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER)
IpczDriverHandle CreateIpczTransportFromEndpoint(
    mojo::PlatformChannelEndpoint endpoint,
    const TransportEndpointTypes& endpoint_types,
    base::Process remote_process = base::Process());

}  // namespace mojo::core

#endif  // MOJO_CORE_EMBEDDER_EMBEDDER_H_
