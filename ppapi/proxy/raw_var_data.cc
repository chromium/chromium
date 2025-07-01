// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/raw_var_data.h"

#include <memory>
#include <unordered_set>

#include "base/check.h"
#include "base/containers/stack.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ipc/ipc_message.h"
#include "ppapi/proxy/ppapi_param_traits.h"
#include "ppapi/shared_impl/array_var.h"
#include "ppapi/shared_impl/dictionary_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_var.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"

using std::make_pair;

namespace ppapi {
namespace proxy {

namespace {

// When sending array buffers, if the size is over 256K, we use shared
// memory instead of sending the data over IPC. Light testing suggests
// shared memory is much faster for 256K and larger messages.
static const uint32_t kMinimumArrayBufferSizeForShmem = 256 * 1024;
static uint32_t g_minimum_array_buffer_size_for_shmem =
    kMinimumArrayBufferSizeForShmem;

struct StackEntry {
  StackEntry(PP_Var v, size_t i) : var(v), data_index(i) {}
  PP_Var var;
  size_t data_index;
};

// For a given PP_Var, returns the RawVarData associated with it, or creates a
// new one if there is no existing one. The data is appended to |data| if it
// is newly created. The index into |data| pointing to the result is returned.
// |visited_map| keeps track of RawVarDatas that have already been created.
size_t GetOrCreateRawVarData(const PP_Var& var,
                             std::unordered_map<int64_t, size_t>* visited_map,
                             std::vector<std::unique_ptr<RawVarData>>* data) {
  if (VarTracker::IsVarTypeRefcounted(var.type)) {
    std::unordered_map<int64_t, size_t>::iterator it =
        visited_map->find(var.value.as_id);
    if (it != visited_map->end()) {
      return it->second;
    } else {
      data->push_back(base::WrapUnique(RawVarData::Create(var.type)));
      (*visited_map)[var.value.as_id] = data->size() - 1;
    }
  } else {
    data->push_back(base::WrapUnique(RawVarData::Create(var.type)));
  }
  return data->size() - 1;
}

bool CanHaveChildren(PP_Var var) {
  return var.type == PP_VARTYPE_ARRAY || var.type == PP_VARTYPE_DICTIONARY;
}

}  // namespace

// RawVarDataGraph ------------------------------------------------------------
RawVarDataGraph::RawVarDataGraph() {
}

RawVarDataGraph::~RawVarDataGraph() {
}

// This function uses a stack-based DFS search to traverse the var graph. Each
// iteration, the top node on the stack examined. If the node has not been
// visited yet (i.e. !initialized()) then it is added to the list of
// |parent_ids| which contains all of the nodes on the path from the start node
// to the current node. Each of that nodes children are examined. If they appear
// in the list of |parent_ids| it means we have a cycle and we return NULL.
// Otherwise, if they haven't been visited yet we add them to the stack, If the
// node at the top of the stack has already been visited, then we pop it off the
// stack and erase it from |parent_ids|.
// static
std::unique_ptr<RawVarDataGraph> RawVarDataGraph::Create(const PP_Var& var,
                                                         PP_Instance instance) {
  std::unique_ptr<RawVarDataGraph> graph(new RawVarDataGraph);
  // Map of |var.value.as_id| to a RawVarData index in RawVarDataGraph.
  std::unordered_map<int64_t, size_t> visited_map;
  std::unordered_set<int64_t> parent_ids;

  base::stack<StackEntry> stack;
  stack.push(StackEntry(var, GetOrCreateRawVarData(var, &visited_map,
                                                   &graph->data_)));

  while (!stack.empty()) {
    PP_Var current_var = stack.top().var;
    RawVarData* current_var_data = graph->data_[stack.top().data_index].get();

    if (current_var_data->initialized()) {
      stack.pop();
      if (CanHaveChildren(current_var))
        parent_ids.erase(current_var.value.as_id);
      continue;
    }

    if (CanHaveChildren(current_var))
      parent_ids.insert(current_var.value.as_id);
    const bool success = current_var_data->Init(current_var, instance);
    CHECK(success);

    // Add child nodes to the stack.
    if (current_var.type == PP_VARTYPE_ARRAY) {
      ArrayVar* array_var = ArrayVar::FromPPVar(current_var);
      CHECK(array_var);
      for (ArrayVar::ElementVector::const_iterator iter =
               array_var->elements().begin();
           iter != array_var->elements().end();
           ++iter) {
        const PP_Var& child = iter->get();
        // If a child of this node is already in parent_ids, we have a cycle so
        // we just return null.
        if (CanHaveChildren(child) && parent_ids.count(child.value.as_id) != 0)
          return nullptr;
        size_t child_id = GetOrCreateRawVarData(child, &visited_map,
                                                &graph->data_);
        static_cast<ArrayRawVarData*>(current_var_data)->AddChild(child_id);
        if (!graph->data_[child_id]->initialized())
          stack.push(StackEntry(child, child_id));
      }
    } else if (current_var.type == PP_VARTYPE_DICTIONARY) {
      DictionaryVar* dict_var = DictionaryVar::FromPPVar(current_var);
      CHECK(dict_var);
      for (DictionaryVar::KeyValueMap::const_iterator iter =
               dict_var->key_value_map().begin();
           iter != dict_var->key_value_map().end();
           ++iter) {
        const PP_Var& child = iter->second.get();
        if (CanHaveChildren(child) && parent_ids.count(child.value.as_id) != 0)
          return nullptr;
        size_t child_id = GetOrCreateRawVarData(child, &visited_map,
                                                &graph->data_);
        static_cast<DictionaryRawVarData*>(
            current_var_data)->AddChild(iter->first, child_id);
        if (!graph->data_[child_id]->initialized())
          stack.push(StackEntry(child, child_id));
      }
    }
  }
  return graph;
}

PP_Var RawVarDataGraph::CreatePPVar(PP_Instance instance) {
  // Create and initialize each node in the graph.
  std::vector<PP_Var> graph;
  for (size_t i = 0; i < data_.size(); ++i)
    graph.push_back(data_[i]->CreatePPVar(instance));
  for (size_t i = 0; i < data_.size(); ++i)
    data_[i]->PopulatePPVar(graph[i], graph);
  // Everything except the root will have one extra ref. Remove that ref.
  for (size_t i = 1; i < data_.size(); ++i)
    ScopedPPVar(ScopedPPVar::PassRef(), graph[i]);
  // The first element is the root.
  return graph[0];
}

void RawVarDataGraph::Write(base::Pickle* m,
                            const HandleWriter& handle_writer) {
  // Write the size, followed by each node in the graph.
  m->WriteUInt32(static_cast<uint32_t>(data_.size()));
  for (size_t i = 0; i < data_.size(); ++i) {
    m->WriteInt(data_[i]->Type());
    data_[i]->Write(m, handle_writer);
  }
}

// static
std::unique_ptr<RawVarDataGraph> RawVarDataGraph::Read(
    const base::Pickle* m,
    base::PickleIterator* iter) {
  std::unique_ptr<RawVarDataGraph> result(new RawVarDataGraph);
  uint32_t size = 0;
  if (!iter->ReadUInt32(&size))
    return nullptr;
  for (uint32_t i = 0; i < size; ++i) {
    int32_t type;
    if (!iter->ReadInt(&type))
      return nullptr;
    PP_VarType var_type = static_cast<PP_VarType>(type);
    result->data_.push_back(base::WrapUnique(RawVarData::Create(var_type)));
    if (!result->data_.back())
      return nullptr;
    if (!result->data_.back()->Read(var_type, m, iter))
      return nullptr;
  }
  return result;
}

std::vector<SerializedHandle*> RawVarDataGraph::GetHandles() {
  std::vector<SerializedHandle*> result;
  for (size_t i = 0; i < data_.size(); ++i) {
    SerializedHandle* handle = data_[i]->GetHandle();
    if (handle)
      result.push_back(handle);
  }
  return result;
}

// static
void RawVarDataGraph::SetMinimumArrayBufferSizeForShmemForTest(
    uint32_t threshold) {
  if (threshold == 0)
    g_minimum_array_buffer_size_for_shmem = kMinimumArrayBufferSizeForShmem;
  else
    g_minimum_array_buffer_size_for_shmem = threshold;
}

// RawVarData ------------------------------------------------------------------

// static
RawVarData* RawVarData::Create(PP_VarType type) {
  switch (type) {
    case PP_VARTYPE_UNDEFINED:
    case PP_VARTYPE_NULL:
    case PP_VARTYPE_BOOL:
    case PP_VARTYPE_INT32:
    case PP_VARTYPE_DOUBLE:
    case PP_VARTYPE_OBJECT:
      return new BasicRawVarData();
    case PP_VARTYPE_STRING:
      return new StringRawVarData();
    case PP_VARTYPE_ARRAY_BUFFER:
      return new ArrayBufferRawVarData();
    case PP_VARTYPE_ARRAY:
      return new ArrayRawVarData();
    case PP_VARTYPE_DICTIONARY:
      return new DictionaryRawVarData();
    case PP_VARTYPE_RESOURCE:
      return new ResourceRawVarData();
  }
  NOTREACHED();
}

RawVarData::RawVarData() : initialized_(false) {
}

RawVarData::~RawVarData() {
}

SerializedHandle* RawVarData::GetHandle() {
  return NULL;
}

// BasicRawVarData -------------------------------------------------------------
BasicRawVarData::BasicRawVarData() {
}

BasicRawVarData::~BasicRawVarData() {
}

PP_VarType BasicRawVarData::Type() {
  return var_.type;
}

bool BasicRawVarData::Init(const PP_Var& var, PP_Instance /*instance*/) {
  var_ = var;
  initialized_ = true;
  return true;
}

PP_Var BasicRawVarData::CreatePPVar(PP_Instance instance) {
  return var_;
}

void BasicRawVarData::PopulatePPVar(const PP_Var& var,
                                    const std::vector<PP_Var>& graph) {
}

void BasicRawVarData::Write(base::Pickle* m,
                            const HandleWriter& handle_writer) {
  switch (var_.type) {
    case PP_VARTYPE_UNDEFINED:
    case PP_VARTYPE_NULL:
      // These don't need any data associated with them other than the type we
      // just serialized.
      break;
    case PP_VARTYPE_BOOL:
      m->WriteBool(PP_ToBool(var_.value.as_bool));
      break;
    case PP_VARTYPE_INT32:
      m->WriteInt(var_.value.as_int);
      break;
    case PP_VARTYPE_DOUBLE:
      IPC::WriteParam(m, var_.value.as_double);
      break;
    case PP_VARTYPE_OBJECT:
      m->WriteInt64(var_.value.as_id);
      break;
    default:
      NOTREACHED();
  }
}

bool BasicRawVarData::Read(PP_VarType type,
                           const base::Pickle* m,
                           base::PickleIterator* iter) {
  PP_Var result;
  result.type = type;
  switch (type) {
    case PP_VARTYPE_UNDEFINED:
    case PP_VARTYPE_NULL:
      // These don't have any data associated with them other than the type we
      // just deserialized.
      break;
    case PP_VARTYPE_BOOL: {
      bool bool_value;
      if (!iter->ReadBool(&bool_value))
        return false;
      result.value.as_bool = PP_FromBool(bool_value);
      break;
    }
    case PP_VARTYPE_INT32:
      if (!iter->ReadInt(&result.value.as_int))
        return false;
      break;
    case PP_VARTYPE_DOUBLE:
      if (!IPC::ReadParam(m, iter, &result.value.as_double))
        return false;
      break;
    case PP_VARTYPE_OBJECT:
      if (!iter->ReadInt64(&result.value.as_id))
        return false;
      break;
    default:
      NOTREACHED();
  }
  var_ = result;
  return true;
}

// StringRawVarData ------------------------------------------------------------
StringRawVarData::StringRawVarData() {
}

StringRawVarData::~StringRawVarData() {
}

PP_VarType StringRawVarData::Type() {
  return PP_VARTYPE_STRING;
}

bool StringRawVarData::Init(const PP_Var& var, PP_Instance /*instance*/) {
  DCHECK(var.type == PP_VARTYPE_STRING);
  StringVar* string_var = StringVar::FromPPVar(var);
  if (!string_var)
    return false;
  data_ = string_var->value();
  initialized_ = true;
  return true;
}

PP_Var StringRawVarData::CreatePPVar(PP_Instance instance) {
  return StringVar::SwapValidatedUTF8StringIntoPPVar(&data_);
}

void StringRawVarData::PopulatePPVar(const PP_Var& var,
                                     const std::vector<PP_Var>& graph) {
}

void StringRawVarData::Write(base::Pickle* m,
                             const HandleWriter& handle_writer) {
  m->WriteString(data_);
}

bool StringRawVarData::Read(PP_VarType type,
                            const base::Pickle* m,
                            base::PickleIterator* iter) {
  if (!iter->ReadString(&data_))
    return false;
  return true;
}

// ArrayBufferRawVarData -------------------------------------------------------
ArrayBufferRawVarData::ArrayBufferRawVarData() {
}

ArrayBufferRawVarData::~ArrayBufferRawVarData() {
}

PP_VarType ArrayBufferRawVarData::Type() {
  return PP_VARTYPE_ARRAY_BUFFER;
}

bool ArrayBufferRawVarData::Init(const PP_Var& var,
                                 PP_Instance instance) {
  DCHECK(var.type == PP_VARTYPE_ARRAY_BUFFER);
  ArrayBufferVar* buffer_var = ArrayBufferVar::FromPPVar(var);
  if (!buffer_var)
    return false;
  bool using_shmem = false;
  if (buffer_var->ByteLength() >= g_minimum_array_buffer_size_for_shmem &&
      instance != 0) {
    int host_handle_id;
    base::UnsafeSharedMemoryRegion plugin_handle;
    using_shmem = buffer_var->CopyToNewShmem(instance,
                                             &host_handle_id,
                                             &plugin_handle);
    if (using_shmem) {
      if (host_handle_id != -1) {
        DCHECK(!plugin_handle.IsValid());
        DCHECK(PpapiGlobals::Get()->IsPluginGlobals());
        type_ = ARRAY_BUFFER_SHMEM_HOST;
        host_shm_handle_id_ = host_handle_id;
      } else {
        DCHECK(plugin_handle.IsValid());
        DCHECK(PpapiGlobals::Get()->IsHostGlobals());
        type_ = ARRAY_BUFFER_SHMEM_PLUGIN;
        plugin_shm_handle_ = SerializedHandle(
            base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
                std::move(plugin_handle)));
      }
    }
  }
  if (!using_shmem) {
    type_ = ARRAY_BUFFER_NO_SHMEM;
    data_ = std::string(static_cast<const char*>(buffer_var->Map()),
                        buffer_var->ByteLength());
  }
  initialized_ = true;
  return true;
}

PP_Var ArrayBufferRawVarData::CreatePPVar(PP_Instance instance) {
  PP_Var result = PP_MakeUndefined();
  switch (type_) {
    case ARRAY_BUFFER_SHMEM_HOST: {
      base::UnsafeSharedMemoryRegion host_handle;
      uint32_t size_in_bytes;
      bool ok =
          PpapiGlobals::Get()->GetVarTracker()->StopTrackingSharedMemoryRegion(
              host_shm_handle_id_, instance, &host_handle, &size_in_bytes);
      if (ok) {
        result = PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
            size_in_bytes, std::move(host_handle));
      } else {
        LOG(ERROR) << "Couldn't find array buffer id: " << host_shm_handle_id_;
        return PP_MakeUndefined();
      }
      break;
    }
    case ARRAY_BUFFER_SHMEM_PLUGIN: {
      auto region_size = plugin_shm_handle_.shmem_region().GetSize();
      result = PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
          region_size, base::UnsafeSharedMemoryRegion::Deserialize(
                           plugin_shm_handle_.TakeSharedMemoryRegion()));
      break;
    }
    case ARRAY_BUFFER_NO_SHMEM: {
      result = PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
          static_cast<uint32_t>(data_.size()), data_.data());
      break;
    }
    default:
      NOTREACHED();
  }
  DCHECK(result.type == PP_VARTYPE_ARRAY_BUFFER);
  return result;
}

void ArrayBufferRawVarData::PopulatePPVar(const PP_Var& var,
                                          const std::vector<PP_Var>& graph) {
}

void ArrayBufferRawVarData::Write(base::Pickle* m,
                                  const HandleWriter& handle_writer) {
  m->WriteInt(type_);
  switch (type_) {
    case ARRAY_BUFFER_SHMEM_HOST:
      m->WriteInt(host_shm_handle_id_);
      break;
    case ARRAY_BUFFER_SHMEM_PLUGIN:
      handle_writer.Run(m, plugin_shm_handle_);
      break;
    case ARRAY_BUFFER_NO_SHMEM:
      m->WriteString(data_);
      break;
  }
}

bool ArrayBufferRawVarData::Read(PP_VarType type,
                                 const base::Pickle* m,
                                 base::PickleIterator* iter) {
  int shmem_type;
  if (!iter->ReadInt(&shmem_type))
    return false;
  type_ = static_cast<ShmemType>(shmem_type);
  switch (type_) {
    case ARRAY_BUFFER_SHMEM_HOST:
      if (!iter->ReadInt(&host_shm_handle_id_))
        return false;
      break;
    case ARRAY_BUFFER_SHMEM_PLUGIN:
      if (!IPC::ReadParam(m, iter, &plugin_shm_handle_)) {
        return false;
      }
      break;
    case ARRAY_BUFFER_NO_SHMEM:
      if (!iter->ReadString(&data_))
        return false;
      break;
    default:
      // We read an invalid ID.
      NOTREACHED();
  }
  return true;
}

SerializedHandle* ArrayBufferRawVarData::GetHandle() {
  if (type_ == ARRAY_BUFFER_SHMEM_PLUGIN && plugin_shm_handle_.IsHandleValid())
    return &plugin_shm_handle_;
  return nullptr;
}

// ArrayRawVarData -------------------------------------------------------------
ArrayRawVarData::ArrayRawVarData() {
}

ArrayRawVarData::~ArrayRawVarData() {
}

void ArrayRawVarData::AddChild(size_t element) {
  children_.push_back(element);
}

PP_VarType ArrayRawVarData::Type() {
  return PP_VARTYPE_ARRAY;
}

bool ArrayRawVarData::Init(const PP_Var& var, PP_Instance /*instance*/) {
  initialized_ = true;
  DCHECK(var.type == PP_VARTYPE_ARRAY);
  initialized_ = true;
  return true;
}

PP_Var ArrayRawVarData::CreatePPVar(PP_Instance instance) {
  return (new ArrayVar())->GetPPVar();
}

void ArrayRawVarData::PopulatePPVar(const PP_Var& var,
                                    const std::vector<PP_Var>& graph) {
  if (var.type != PP_VARTYPE_ARRAY) {
    NOTREACHED();
  }
  ArrayVar* array_var = ArrayVar::FromPPVar(var);
  DCHECK(array_var->elements().empty());
  for (size_t i = 0; i < children_.size(); ++i)
    array_var->elements().push_back(ScopedPPVar(graph[children_[i]]));
}

void ArrayRawVarData::Write(base::Pickle* m,
                            const HandleWriter& handle_writer) {
  m->WriteUInt32(static_cast<uint32_t>(children_.size()));
  for (size_t i = 0; i < children_.size(); ++i)
    m->WriteUInt32(static_cast<uint32_t>(children_[i]));
}

bool ArrayRawVarData::Read(PP_VarType type,
                           const base::Pickle* m,
                           base::PickleIterator* iter) {
  uint32_t size;
  if (!iter->ReadUInt32(&size))
    return false;
  for (uint32_t i = 0; i < size; ++i) {
    uint32_t index;
    if (!iter->ReadUInt32(&index))
      return false;
    children_.push_back(index);
  }
  return true;
}

// DictionaryRawVarData --------------------------------------------------------
DictionaryRawVarData::DictionaryRawVarData() {
}

DictionaryRawVarData::~DictionaryRawVarData() {
}

void DictionaryRawVarData::AddChild(const std::string& key,
                                    size_t value) {
  children_.push_back(make_pair(key, value));
}

PP_VarType DictionaryRawVarData::Type() {
  return PP_VARTYPE_DICTIONARY;
}

bool DictionaryRawVarData::Init(const PP_Var& var, PP_Instance /*instance*/) {
  DCHECK(var.type == PP_VARTYPE_DICTIONARY);
  initialized_ = true;
  return true;
}

PP_Var DictionaryRawVarData::CreatePPVar(PP_Instance instance) {
  return (new DictionaryVar())->GetPPVar();
}

void DictionaryRawVarData::PopulatePPVar(const PP_Var& var,
                                         const std::vector<PP_Var>& graph) {
  if (var.type != PP_VARTYPE_DICTIONARY) {
    NOTREACHED();
  }
  DictionaryVar* dictionary_var = DictionaryVar::FromPPVar(var);
  DCHECK(dictionary_var->key_value_map().empty());
  for (size_t i = 0; i < children_.size(); ++i) {
    bool success = dictionary_var->SetWithStringKey(children_[i].first,
                                                    graph[children_[i].second]);
    DCHECK(success);
  }
}

void DictionaryRawVarData::Write(base::Pickle* m,
                                 const HandleWriter& handle_writer) {
  m->WriteUInt32(static_cast<uint32_t>(children_.size()));
  for (size_t i = 0; i < children_.size(); ++i) {
    m->WriteString(children_[i].first);
    m->WriteUInt32(static_cast<uint32_t>(children_[i].second));
  }
}

bool DictionaryRawVarData::Read(PP_VarType type,
                                const base::Pickle* m,
                                base::PickleIterator* iter) {
  uint32_t size;
  if (!iter->ReadUInt32(&size))
    return false;
  for (uint32_t i = 0; i < size; ++i) {
    std::string key;
    uint32_t value;
    if (!iter->ReadString(&key))
      return false;
    if (!iter->ReadUInt32(&value))
      return false;
    children_.push_back(make_pair(key, value));
  }
  return true;
}

// ResourceRawVarData ----------------------------------------------------------
ResourceRawVarData::ResourceRawVarData()
    : pp_resource_(0),
      pending_renderer_host_id_(0),
      pending_browser_host_id_(0) {}

ResourceRawVarData::~ResourceRawVarData() {
}

PP_VarType ResourceRawVarData::Type() {
  return PP_VARTYPE_RESOURCE;
}

bool ResourceRawVarData::Init(const PP_Var& var, PP_Instance /*instance*/) {
  DCHECK(var.type == PP_VARTYPE_RESOURCE);
  ResourceVar* resource_var = ResourceVar::FromPPVar(var);
  if (!resource_var)
    return false;
  pp_resource_ = resource_var->GetPPResource();
  const IPC::Message* message = resource_var->GetCreationMessage();
  if (message)
    creation_message_ = std::make_unique<IPC::Message>(*message);
  else
    creation_message_.reset();
  pending_renderer_host_id_ = resource_var->GetPendingRendererHostId();
  pending_browser_host_id_ = resource_var->GetPendingBrowserHostId();
  initialized_ = true;
  return true;
}

PP_Var ResourceRawVarData::CreatePPVar(PP_Instance instance) {
  // If this is not a pending resource host, just create the var.
  if (pp_resource_ || !creation_message_) {
    return PpapiGlobals::Get()->GetVarTracker()->MakeResourcePPVar(
        pp_resource_);
  }

  // This is a pending resource host, so create the resource and var.
  return PpapiGlobals::Get()->GetVarTracker()->MakeResourcePPVarFromMessage(
      instance,
      *creation_message_,
      pending_renderer_host_id_,
      pending_browser_host_id_);
}

void ResourceRawVarData::PopulatePPVar(const PP_Var& var,
                                       const std::vector<PP_Var>& graph) {
}

void ResourceRawVarData::Write(base::Pickle* m,
                               const HandleWriter& handle_writer) {
  m->WriteInt(static_cast<int>(pp_resource_));
  m->WriteInt(pending_renderer_host_id_);
  m->WriteInt(pending_browser_host_id_);
  m->WriteBool(!!creation_message_);
  if (creation_message_)
    IPC::WriteParam(m, *creation_message_);
}

bool ResourceRawVarData::Read(PP_VarType type,
                              const base::Pickle* m,
                              base::PickleIterator* iter) {
  int value;
  if (!iter->ReadInt(&value))
    return false;
  pp_resource_ = static_cast<PP_Resource>(value);
  if (!iter->ReadInt(&pending_renderer_host_id_))
    return false;
  if (!iter->ReadInt(&pending_browser_host_id_))
    return false;
  bool has_creation_message;
  if (!iter->ReadBool(&has_creation_message))
    return false;
  if (has_creation_message) {
    creation_message_ = std::make_unique<IPC::Message>();
    if (!IPC::ReadParam(m, iter, creation_message_.get()))
      return false;
  } else {
    creation_message_.reset();
  }
  return true;
}

}  // namespace proxy
}  // namespace ppapi
