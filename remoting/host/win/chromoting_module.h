// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_CHROMOTING_MODULE_H_
#define REMOTING_HOST_WIN_CHROMOTING_MODULE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/win/atl.h"
#include "base/win/scoped_com_initializer.h"

// chromoting_lib.h contains MIDL-generated declarations.
#include "remoting/host/win/chromoting_lib.h"

namespace base {
namespace win {
class ScopedCOMInitializer;
}  // namespace win
}  // namespace base

namespace remoting {

class AutoThreadTaskRunner;

// A custom version of |CAtlModuleT<>| that registers only those classes which
// registration entries are passed to the constructor. |ChromotingModule| runs
// |MessageLoop| allowing Chromium code to post tasks to it. Unlike
// |CAtlExeModuleT<>|, |ChromotingModule| shuts itself down immediately once
// the last COM object is released.
class ChromotingModule : public ATL::CAtlModuleT<ChromotingModule> {
 public:
  // Initializes the module. |classes| and |classes_end| must outlive |this|.
  ChromotingModule(ATL::_ATL_OBJMAP_ENTRY* classes,
                   ATL::_ATL_OBJMAP_ENTRY* classes_end);

  ChromotingModule(const ChromotingModule&) = delete;
  ChromotingModule& operator=(const ChromotingModule&) = delete;

  ~ChromotingModule() override;

  // Returns the task runner used by the module. Returns nullptr if the task
  // runner hasn't been registered yet or if the server is shutting down.
  static scoped_refptr<AutoThreadTaskRunner> task_runner();

  // Registers COM classes and runs the main message loop until there are
  // components using it.
  bool Run();

  // ATL::CAtlModuleT<> overrides
  LONG Unlock() override;

  DECLARE_LIBID(LIBID_ChromotingLib)

 private:
  // Registers/unregisters class objects from |classes_| - |classes_end_|.
  HRESULT RegisterClassObjects(DWORD class_context, DWORD flags);
  HRESULT RevokeClassObjects();

  // Used to initialize COM library.
  base::win::ScopedCOMInitializer com_initializer_;

  // Point to the vector of classes registered by this module.
  raw_ptr<ATL::_ATL_OBJMAP_ENTRY> classes_;
  raw_ptr<ATL::_ATL_OBJMAP_ENTRY> classes_end_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_CHROMOTING_MODULE_H_
