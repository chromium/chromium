// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_RAW_VAR_DATA_H_
#define PPAPI_PROXY_RAW_VAR_DATA_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/ppapi_param_traits.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/serialized_handle.h"

namespace base {
class Pickle;
class PickleIterator;
}

namespace IPC {
class Message;
}

namespace ppapi {
namespace proxy {

class RawVarData;

typedef base::RepeatingCallback<void(base::Pickle*, const SerializedHandle&)>
    HandleWriter;

// Contains the data associated with a graph of connected PP_Vars. Useful for
// serializing/deserializing a graph of PP_Vars. First we compute the transitive
// closure of the given PP_Var to find all PP_Vars which are referenced by that
// var. A RawVarData object is created for each of these vars. We then write
// data contained in each RawVarData to the message. The format looks like this:
//    idx | size     | (number of vars in the graph)
//     0  | var type |
//        | var data |
//     1  | var type |
//        | var data |
//     2  | var type |
//        | var data |
//        |   ....   |
//
// Vars that reference other vars (such as Arrays or Dictionaries) use indices
// into the message to denote which PP_Var is pointed to.
class PPAPI_PROXY_EXPORT RawVarDataGraph {
 public:
  // Construct a RawVarDataGraph from a given root PP_Var. A null pointer
  // is returned upon failure.
  static std::unique_ptr<RawVarDataGraph> Create(const PP_Var& var,
                                                 PP_Instance instance);

  // Constructs an empty RawVarDataGraph.
  RawVarDataGraph();

  RawVarDataGraph(const RawVarDataGraph&) = delete;
  RawVarDataGraph& operator=(const RawVarDataGraph&) = delete;

  ~RawVarDataGraph();

  // Construct a new PP_Var from the graph. All of the PP_Vars referenced by
  // the returned PP_Var are also constructed. Each PP_Var created has a
  // ref-count equal to the number of references it has in the graph of vars.
  // The returned var (the "root") has one additional reference.
  PP_Var CreatePPVar(PP_Instance instance);

  // Write the graph to a message using the given HandleWriter.
  void Write(base::Pickle* m, const HandleWriter& handle_writer);

  // Create a RawVarDataGraph from the given message.
  static std::unique_ptr<RawVarDataGraph> Read(const base::Pickle* m,
                                               base::PickleIterator* iter);

  // Returns a vector of SerializedHandles associated with this RawVarDataGraph.
  // Ownership of the pointers remains with the elements of the RawVarDataGraph.
  std::vector<SerializedHandle*> GetHandles();

  // Sets the threshold size at which point we switch from transmitting
  // array buffers in IPC messages to using shared memory. This is only used
  // for testing purposes where we need to transmit small buffers using shmem
  // (in order to have fast tests).
  static void SetMinimumArrayBufferSizeForShmemForTest(uint32_t threshold);

 private:
  // A list of the nodes in the graph.
  std::vector<std::unique_ptr<RawVarData>> data_;
};

// Abstract base class for the data contained in a PP_Var.
class RawVarData {
 public:
  // Create a new, empty RawVarData for the given type.
  static RawVarData* Create(PP_VarType type);
  RawVarData();
  virtual ~RawVarData();

  // Returns the type of the PP_Var represented by the RawVarData.
  virtual PP_VarType Type() = 0;

  // Initializes a RawVarData from a PP_Var. Returns true on success.
  virtual bool Init(const PP_Var& var, PP_Instance instance) = 0;

  // Create a PP_Var from the raw data contained in this object.
  virtual PP_Var CreatePPVar(PP_Instance instance) = 0;
  // Some PP_Vars may require 2-step initialization. For example, they may
  // reference other PP_Vars which had not yet been created when |CreatePPVar|
  // was called. The original var created with |CreatePPVar| is passed back in,
  // along with the graph it is a part of to be initialized.
  virtual void PopulatePPVar(const PP_Var& var,
                             const std::vector<PP_Var>& graph) = 0;

  // Writes the RawVarData to a message.
  virtual void Write(base::Pickle* m, const HandleWriter& handle_writer) = 0;
  // Reads the RawVarData from a message. Returns true on success.
  virtual bool Read(PP_VarType type,
                    const base::Pickle* m,
                    base::PickleIterator* iter) = 0;

  // Returns a SerializedHandle associated with this RawVarData or NULL if none
  // exists. Ownership of the pointer remains with the RawVarData.
  virtual SerializedHandle* GetHandle();

  bool initialized() { return initialized_; }

 protected:
  bool initialized_;
};

// A RawVarData class for PP_Vars which are value types.
class BasicRawVarData : public RawVarData {
 public:
  BasicRawVarData();
  ~BasicRawVarData() override;

  // RawVarData implementation.
  PP_VarType Type() override;
  bool Init(const PP_Var& var, PP_Instance instance) override;
  PP_Var CreatePPVar(PP_Instance instance) override;
  void PopulatePPVar(const PP_Var& var,
                     const std::vector<PP_Var>& graph) override;
  void Write(base::Pickle* m, const HandleWriter& handle_writer) override;
  bool Read(PP_VarType type,
            const base::Pickle* m,
            base::PickleIterator* iter) override;

 private:
  PP_Var var_;
};

// A RawVarData class for string PP_Vars.
class StringRawVarData : public RawVarData {
 public:
  StringRawVarData();
  ~StringRawVarData() override;

  // RawVarData implementation.
  PP_VarType Type() override;
  bool Init(const PP_Var& var, PP_Instance instance) override;
  PP_Var CreatePPVar(PP_Instance instance) override;
  void PopulatePPVar(const PP_Var& var,
                     const std::vector<PP_Var>& graph) override;
  void Write(base::Pickle* m, const HandleWriter& handle_writer) override;
  bool Read(PP_VarType type,
            const base::Pickle* m,
            base::PickleIterator* iter) override;

 private:
  // The data in the string.
  std::string data_;
};

// A RawVarData class for array buffer PP_Vars.
class ArrayBufferRawVarData : public RawVarData {
 public:
  // Enum for array buffer message types.
  enum ShmemType {
    ARRAY_BUFFER_NO_SHMEM,
    ARRAY_BUFFER_SHMEM_HOST,
    ARRAY_BUFFER_SHMEM_PLUGIN,
  };

  ArrayBufferRawVarData();
  ~ArrayBufferRawVarData() override;

  // RawVarData implementation.
  PP_VarType Type() override;
  bool Init(const PP_Var& var, PP_Instance instance) override;
  PP_Var CreatePPVar(PP_Instance instance) override;
  void PopulatePPVar(const PP_Var& var,
                     const std::vector<PP_Var>& graph) override;
  void Write(base::Pickle* m, const HandleWriter& handle_writer) override;
  bool Read(PP_VarType type,
            const base::Pickle* m,
            base::PickleIterator* iter) override;
  SerializedHandle* GetHandle() override;

 private:
  // The type of the storage underlying the array buffer.
  ShmemType type_;
  // The data in the buffer. Valid for |type_| == ARRAY_BUFFER_NO_SHMEM.
  std::string data_;
  // Host shmem handle. Valid for |type_| == ARRAY_BUFFER_SHMEM_HOST.
  int host_shm_handle_id_;
  // Plugin shmem handle. Valid for |type_| == ARRAY_BUFFER_SHMEM_PLUGIN.
  SerializedHandle plugin_shm_handle_;
};

// A RawVarData class for array PP_Vars.
class ArrayRawVarData : public RawVarData {
 public:
  ArrayRawVarData();
  ~ArrayRawVarData() override;

  void AddChild(size_t element);

  // RawVarData implementation.
  PP_VarType Type() override;
  bool Init(const PP_Var& var, PP_Instance instance) override;
  PP_Var CreatePPVar(PP_Instance instance) override;
  void PopulatePPVar(const PP_Var& var,
                     const std::vector<PP_Var>& graph) override;
  void Write(base::Pickle* m, const HandleWriter& handle_writer) override;
  bool Read(PP_VarType type,
            const base::Pickle* m,
            base::PickleIterator* iter) override;

 private:
  std::vector<size_t> children_;
};

// A RawVarData class for dictionary PP_Vars.
class DictionaryRawVarData : public RawVarData {
 public:
  DictionaryRawVarData();
  ~DictionaryRawVarData() override;

  void AddChild(const std::string& key, size_t value);

  // RawVarData implementation.
  PP_VarType Type() override;
  bool Init(const PP_Var& var, PP_Instance instance) override;
  PP_Var CreatePPVar(PP_Instance instance) override;
  void PopulatePPVar(const PP_Var& var,
                     const std::vector<PP_Var>& graph) override;
  void Write(base::Pickle* m, const HandleWriter& handle_writer) override;
  bool Read(PP_VarType type,
            const base::Pickle* m,
            base::PickleIterator* iter) override;

 private:
  std::vector<std::pair<std::string, size_t> > children_;
};

// A RawVarData class for resource PP_Vars.
// This class does not hold a reference on the PP_Resource that is being
// serialized. If sending a resource from the plugin to the host, the plugin
// should not release the ResourceVar before sending the serialized message to
// the host, and the host should immediately consume the ResourceVar before
// processing further messages.
class ResourceRawVarData : public RawVarData {
 public:
  ResourceRawVarData();
  ~ResourceRawVarData() override;

  // RawVarData implementation.
  PP_VarType Type() override;
  bool Init(const PP_Var& var, PP_Instance instance) override;
  PP_Var CreatePPVar(PP_Instance instance) override;
  void PopulatePPVar(const PP_Var& var,
                     const std::vector<PP_Var>& graph) override;
  void Write(base::Pickle* m, const HandleWriter& handle_writer) override;
  bool Read(PP_VarType type,
            const base::Pickle* m,
            base::PickleIterator* iter) override;

 private:
  // Resource ID in the plugin. If one has not yet been created, this is 0.
  // This is a borrowed reference; the resource's refcount is not incremented.
  PP_Resource pp_resource_;

  // Pending resource host ID in the renderer.
  int pending_renderer_host_id_;

  // Pending resource host ID in the browser.
  int pending_browser_host_id_;

  // A message containing information about how to create a plugin-side
  // resource. The message type will vary based on the resource type, and will
  // usually contain a pending resource host ID, and other required information.
  // If the resource was created directly, this is NULL.
  std::unique_ptr<IPC::Message> creation_message_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_RAW_VAR_DATA_H_
