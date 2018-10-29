// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/producer_client.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "third_party/perfetto/include/perfetto/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/tracing/core/shared_memory_arbiter.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_writer.h"

namespace tracing {

namespace {

scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner() {
  return base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
}

// We never destroy the taskrunner as we may need it for cleanup
// of TraceWriters in TLS, which could happen after the ProducerClient
// is deleted.
PerfettoTaskRunner* GetPerfettoTaskRunner() {
  static base::NoDestructor<PerfettoTaskRunner> task_runner(CreateTaskRunner());
  return task_runner.get();
}

}  // namespace

ProducerClient::DataSourceBase::DataSourceBase(const std::string& name)
    : name_(name) {
  DCHECK(!name.empty());
}

ProducerClient::DataSourceBase::~DataSourceBase() = default;

void ProducerClient::DataSourceBase::StartTracingWithID(
    uint64_t data_source_id,
    ProducerClient* producer_client,
    const mojom::DataSourceConfig& data_source_config) {
  data_source_id_ = data_source_id;
  StartTracing(producer_client, data_source_config);
}

ProducerClient::ProducerClient() : weak_ptr_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProducerClient::~ProducerClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
void ProducerClient::DeleteSoonForTesting(
    std::unique_ptr<ProducerClient> producer_client) {
  GetTaskRunner()->DeleteSoon(FROM_HERE, std::move(producer_client));
}

// static
base::SequencedTaskRunner* ProducerClient::GetTaskRunner() {
  auto* task_runner = GetPerfettoTaskRunner()->task_runner();
  DCHECK(task_runner);
  return task_runner;
}

// static
void ProducerClient::ResetTaskRunnerForTesting() {
  GetPerfettoTaskRunner()->ResetTaskRunnerForTesting(CreateTaskRunner());
}

void ProducerClient::CreateMojoMessagepipes(
    MessagepipesReadyCallback callback) {
  auto origin_task_runner = base::SequencedTaskRunnerHandle::Get();
  DCHECK(origin_task_runner);
  mojom::ProducerClientPtr producer_client;
  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProducerClient::CreateMojoMessagepipesOnSequence,
                     base::Unretained(this), origin_task_runner,
                     std::move(callback), mojo::MakeRequest(&producer_client),
                     std::move(producer_client)));
}

// The Mojo binding should run on the same sequence as the one we get
// callbacks from Perfetto on, to avoid additional PostTasks.
void ProducerClient::CreateMojoMessagepipesOnSequence(
    scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
    MessagepipesReadyCallback callback,
    mojom::ProducerClientRequest producer_client_request,
    mojom::ProducerClientPtr producer_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  binding_ = std::make_unique<mojo::Binding<mojom::ProducerClient>>(
      this, std::move(producer_client_request));
  origin_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(producer_client),
                                mojo::MakeRequest(&producer_host_)));
}

void ProducerClient::AddDataSource(DataSourceBase* data_source) {
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProducerClient::AddDataSourceOnSequence,
                                base::Unretained(this), data_source));
}

void ProducerClient::AddDataSourceOnSequence(DataSourceBase* data_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_sources_.insert(data_source);
  auto new_registration = mojom::DataSourceRegistration::New();
  new_registration->name = data_source->name();
  new_registration->will_notify_on_stop = true;
  producer_host_->RegisterDataSource(std::move(new_registration));
}

void ProducerClient::OnTracingStart(
    mojo::ScopedSharedBufferHandle shared_memory) {
  // TODO(oysteine): In next CLs plumb this through the service.
  const size_t kShmemBufferPageSize = 4096;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(producer_host_);
  if (!shared_memory_) {
    shared_memory_ =
        std::make_unique<MojoSharedMemory>(std::move(shared_memory));

    shared_memory_arbiter_ = perfetto::SharedMemoryArbiter::CreateInstance(
        shared_memory_.get(), kShmemBufferPageSize, this,
        GetPerfettoTaskRunner());
  } else {
    // TODO(oysteine): This is assuming the SMB is the same, currently. Swapping
    // out SharedMemoryBuffers would require more thread synchronization.
    DCHECK_EQ(shared_memory_->shared_buffer()->value(), shared_memory->value());
  }
}

void ProducerClient::StartDataSource(
    uint64_t id,
    mojom::DataSourceConfigPtr data_source_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_source_config);

  // TODO(oysteine): Support concurrent tracing sessions.
  for (auto* data_source : data_sources_) {
    if (data_source->name() == data_source_config->name) {
      data_source->StartTracingWithID(id, this, *data_source_config);
      return;
    }
  }
}

void ProducerClient::StopDataSource(uint64_t id,
                                    StopDataSourceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto* data_source : data_sources_) {
    if (data_source->data_source_id() == id) {
      data_source->StopTracing(std::move(callback));
      return;
    }
  }

  LOG(FATAL) << "Invalid data source ID.";
}

void ProducerClient::Flush(uint64_t flush_request_id,
                           const std::vector<uint64_t>& data_source_ids) {
  pending_replies_for_latest_flush_ = {flush_request_id,
                                       data_source_ids.size()};

  // N^2, optimize once there's more than a couple of possible data sources.
  for (auto* data_source : data_sources_) {
    if (std::find(data_source_ids.begin(), data_source_ids.end(),
                  data_source->data_source_id()) != data_source_ids.end()) {
      data_source->Flush(base::BindRepeating(
          [](base::WeakPtr<ProducerClient> weak_ptr, uint64_t id) {
            if (weak_ptr) {
              weak_ptr->NotifyFlushComplete(id);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), flush_request_id));
    }
  }
}

void ProducerClient::RegisterDataSource(const perfetto::DataSourceDescriptor&) {
  NOTREACHED();
}

void ProducerClient::UnregisterDataSource(const std::string& name) {
  NOTREACHED();
}

void ProducerClient::NotifyDataSourceStopped(
    perfetto::DataSourceInstanceID id) {
  NOTREACHED();
}

void ProducerClient::CommitData(const perfetto::CommitDataRequest& commit,
                                CommitDataCallback callback) {
  // The CommitDataRequest which the SharedMemoryArbiter uses to
  // signal Perfetto that individual chunks have finished being
  // written and is ready for consumption, needs to be serialized
  // into the corresponding Mojo class and sent over to the
  // service-side.
  auto new_data_request = mojom::CommitDataRequest::New();

  new_data_request->flush_request_id = commit.flush_request_id();
  for (auto& chunk : commit.chunks_to_move()) {
    auto new_chunk = mojom::ChunksToMove::New();
    new_chunk->page = chunk.page();
    new_chunk->chunk = chunk.chunk();
    new_chunk->target_buffer = chunk.target_buffer();
    new_data_request->chunks_to_move.push_back(std::move(new_chunk));
  }

  for (auto& chunk_patch : commit.chunks_to_patch()) {
    auto new_chunk_patch = mojom::ChunksToPatch::New();
    new_chunk_patch->target_buffer = chunk_patch.target_buffer();
    new_chunk_patch->writer_id = chunk_patch.writer_id();
    new_chunk_patch->chunk_id = chunk_patch.chunk_id();

    for (auto& patch : chunk_patch.patches()) {
      auto new_patch = mojom::ChunkPatch::New();
      new_patch->offset = patch.offset();
      new_patch->data = patch.data();
      new_chunk_patch->patches.push_back(std::move(new_patch));
    }

    new_chunk_patch->has_more_patches = chunk_patch.has_more_patches();
    new_data_request->chunks_to_patch.push_back(std::move(new_chunk_patch));
  }

  // TODO(oysteine): Remove the PostTask once Perfetto is fixed to always call
  // CommitData on its provided TaskRunner, right now it'll call it on whatever
  // thread is requesting a new chunk when the SharedMemoryBuffer is full. Until
  // then this is technically not threadsafe on shutdown (when the
  // ProducerClient gets destroyed) but should be okay while the Perfetto
  // integration is behind a flag.
  if (GetTaskRunner()->RunsTasksInCurrentSequence()) {
    producer_host_->CommitData(std::move(new_data_request));
  } else {
    GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProducerClient::CommitDataOnSequence,
                       base::Unretained(this), std::move(new_data_request)));
  }
}

void ProducerClient::CommitDataOnSequence(mojom::CommitDataRequestPtr request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  producer_host_->CommitData(std::move(request));
}

perfetto::SharedMemory* ProducerClient::shared_memory() const {
  return shared_memory_.get();
}

size_t ProducerClient::shared_buffer_page_size_kb() const {
  NOTREACHED();
  return 0;
}

void ProducerClient::NotifyFlushComplete(perfetto::FlushRequestID id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_replies_for_latest_flush_.first != id) {
    // Ignore; completed flush was for an earlier request.
    return;
  }

  DCHECK_NE(pending_replies_for_latest_flush_.second, 0u);
  if (--pending_replies_for_latest_flush_.second == 0) {
    producer_host_->NotifyFlushComplete(id);
  }
}

std::unique_ptr<perfetto::TraceWriter> ProducerClient::CreateTraceWriter(
    perfetto::BufferID target_buffer) {
  DCHECK(shared_memory_arbiter_);
  return shared_memory_arbiter_->CreateTraceWriter(target_buffer);
}

}  // namespace tracing
