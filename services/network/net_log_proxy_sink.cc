// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/net_log_proxy_sink.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/message.h"

namespace network {

NetLogProxySink::NetLogProxySink()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  // Initialize a WeakPtr instance that can be safely referred to from other
  // threads when binding tasks posted back to this thread.
  weak_this_ = weak_factory_.GetWeakPtr();
  net::NetLog::Get()->AddCaptureModeObserver(this);
}

NetLogProxySink::~NetLogProxySink() {
  net::NetLog::Get()->RemoveCaptureModeObserver(this);
}

void NetLogProxySink::AttachSource(
    mojo::PendingRemote<network::mojom::NetLogProxySource> proxy_source_remote,
    mojo::PendingReceiver<network::mojom::NetLogProxySink>
        proxy_sink_receiver) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Initialize remote with current capturing state. (Netlog capturing might
  // already be active when NetLogProxySource gets attached.)
  mojo::Remote<network::mojom::NetLogProxySource> bound_remote(
      std::move(proxy_source_remote));
  bound_remote->UpdateCaptureModes(GetObserverCaptureModes());

  proxy_source_remotes_.Add(std::move(bound_remote));
  proxy_sink_receivers_.Add(this, std::move(proxy_sink_receiver));
}

void NetLogProxySink::OnCaptureModeUpdated(net::NetLogCaptureModeSet modes) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&NetLogProxySink::OnCaptureModeUpdated,
                                  weak_this_, modes));
    return;
  }

  for (const auto& source : proxy_source_remotes_) {
    source->UpdateCaptureModes(modes);
  }
}

void NetLogProxySink::AddEntry(uint32_t type,
                               const net::NetLogSource& net_log_source,
                               net::NetLogEventPhase phase,
                               base::TimeTicks time,
                               base::Value::Dict params) {
  // Note: There is a possible race condition, where the NetLog capture mode
  // changes, but the other process is still sending events for the old capture
  // mode, and thus might log events with a higher than expected capture mode.
  // (But if capturing is completely disabled and the other side still has some
  // events in the pipe, AddEntryWithMaterializedParams will do nothing, since
  // that implies no observers are registered.)
  // TODO(mattm): Remote side could send the capture mode along with the event,
  // and then check here before logging that the current capture mode still is
  // compatible.

  // In general it is legal to have a NetLogSource that is
  // unbound (represented by kInvalidId), so it is not checked by the typemap
  // traits. However, net log events should never be materialized with an
  // unbound source, so NetLogProxySink expects to only receive a bound
  // NetLogSource.
  if (net_log_source.id == net::NetLogSource::kInvalidId) {
    mojo::ReportBadMessage("invalid NetLogSource");
    return;
  }

  AddEntryAtTimeWithMaterializedParams(static_cast<net::NetLogEventType>(type),
                                       net_log_source, phase, time,
                                       std::move(params));
}

}  // namespace network
