// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_flash_message_loop_impl.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "ppapi/c/pp_errors.h"
#include "third_party/blink/public/platform/web_scoped_page_pauser.h"

using ppapi::thunk::PPB_Flash_MessageLoop_API;

namespace content {

class PPB_Flash_MessageLoop_Impl::State
    : public base::RefCounted<PPB_Flash_MessageLoop_Impl::State> {
 public:
  explicit State() : result_(PP_OK), run_called_(false) {}

  int32_t result() const { return result_; }
  void set_result(int32_t result) { result_ = result; }

  bool run_called() const { return run_called_; }
  void set_run_called() { run_called_ = true; }

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }
  base::OnceClosure& quit_closure() { return quit_closure_; }

  const RunFromHostProxyCallback& run_callback() const { return run_callback_; }
  void set_run_callback(const RunFromHostProxyCallback& run_callback) {
    run_callback_ = run_callback;
  }

 private:
  friend class base::RefCounted<State>;
  virtual ~State() {}

  int32_t result_;
  bool run_called_;
  base::OnceClosure quit_closure_;
  RunFromHostProxyCallback run_callback_;
};

PPB_Flash_MessageLoop_Impl::PPB_Flash_MessageLoop_Impl(PP_Instance instance)
    : Resource(ppapi::OBJECT_IS_IMPL, instance), state_(new State()) {}

PPB_Flash_MessageLoop_Impl::~PPB_Flash_MessageLoop_Impl() {
  // It is a no-op if either Run() hasn't been called or Quit() has been called
  // to balance the call to Run().
  InternalQuit(PP_ERROR_ABORTED);
}

// static
PP_Resource PPB_Flash_MessageLoop_Impl::Create(PP_Instance instance) {
  return (new PPB_Flash_MessageLoop_Impl(instance))->GetReference();
}

PPB_Flash_MessageLoop_API*
PPB_Flash_MessageLoop_Impl::AsPPB_Flash_MessageLoop_API() {
  return this;
}

int32_t PPB_Flash_MessageLoop_Impl::Run() {
  return InternalRun(RunFromHostProxyCallback());
}

void PPB_Flash_MessageLoop_Impl::RunFromHostProxy(
    const RunFromHostProxyCallback& callback) {
  InternalRun(callback);
}

void PPB_Flash_MessageLoop_Impl::Quit() { InternalQuit(PP_OK); }

int32_t PPB_Flash_MessageLoop_Impl::InternalRun(
    const RunFromHostProxyCallback& callback) {
  if (state_->run_called()) {
    if (!callback.is_null())
      callback.Run(PP_ERROR_FAILED);
    return PP_ERROR_FAILED;
  }
  state_->set_run_called();
  state_->set_run_callback(callback);

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  state_->set_quit_closure(run_loop.QuitClosure());

  // It is possible that the PPB_Flash_MessageLoop_Impl object has been
  // destroyed when the nested run loop exits.
  scoped_refptr<State> state_protector(state_);
  {
    std::unique_ptr<blink::WebScopedPagePauser> pauser =
        blink::WebScopedPagePauser::Create();

    run_loop.Run();
  }
  // Don't access data members of the class below.

  return state_protector->result();
}

void PPB_Flash_MessageLoop_Impl::InternalQuit(int32_t result) {
  if (!state_->run_called() || state_->quit_closure().is_null())
    return;
  state_->set_result(result);

  std::move(state_->quit_closure()).Run();

  if (!state_->run_callback().is_null())
    state_->run_callback().Run(result);
}

}  // namespace content
