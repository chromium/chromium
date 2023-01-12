// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_JAVA_HEAP_PROFILER_HPROF_INSTANCES_ANDROID_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_JAVA_HEAP_PROFILER_HPROF_INSTANCES_ANDROID_H_

#include <ostream>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_data_type_android.h"

namespace tracing {

// The base attributes across each type of Instance to that are needed to create
// heap_graph objects. Each of ClassInstance, ClassObject, ObjectArrayInstance,
// and PrimitiveArrayInstance all contain a |base_instance| of type Instance.
struct COMPONENT_EXPORT(TRACING_CPP) Instance {
  // Reference from another instance where the current instance is defined.
  struct Reference {
    std::string referred_by_name;
    uint64_t referred_from_object_id;
  };

  explicit Instance(uint64_t object_id);
  Instance(uint64_t object_id, const std::string& type_name);
  Instance(uint64_t object_id, uint64_t size);
  Instance(uint64_t object_id, uint64_t size, const std::string& type_name);
  Instance(const Instance& other);
  ~Instance();

  void AddReferenceFrom(const std::string& name, uint64_t obj_id);
  void AddReferenceTo(const std::string& name, uint64_t obj_id);

  //  Only set in first pass for ClassObject and PrimitiveArrayInstances.
  std::string type_name;
  const uint64_t object_id;  // Always set in first pass.
  uint64_t size = 0;         // Set in first pass except for ClassObject
  std::vector<Reference> referred_by;  // Always set on second pass.
  std::vector<Reference> referred_to;
};

// A single instance of a particular class. There can be multiple class
// instances of a single class.
struct COMPONENT_EXPORT(TRACING_CPP) ClassInstance {
  ClassInstance(uint64_t object_id,
                uint64_t class_id,
                uint32_t temp_data_position);

  Instance base_instance;
  const uint64_t class_id;

  // When resolving references from one reference to another, need to reset
  // the hprof offset to temp_data_position and read in the bytes located at
  // this specific location within the buffer. The bytes here represent the
  // instance fields of the current class instance. The parser will make a note
  // of a reference from any instance fields that are objects to the current
  // ClassInstance.
  const uint32_t temp_data_position;
};

// Stores data about a static or instance field within a class object.
struct COMPONENT_EXPORT(TRACING_CPP) Field {
  Field(const std::string& name, DataType type, uint64_t object_id);

  std::string name;
  DataType type;
  uint64_t object_id;
};

// An instance that contains the information associated with a given class's
// architecture. There can only be one class object per class.
struct COMPONENT_EXPORT(TRACING_CPP) ClassObject {
  ~ClassObject();
  ClassObject(uint64_t object_id, const std::string& type_name);

  Instance base_instance;
  uint64_t super_class_id = 0;  // Set in first pass.
  uint64_t instance_size = 0;
  std::vector<Field> instance_fields;
  std::vector<Field> static_fields;
};

// An instance that is an array of object_ids of a specific class object
struct COMPONENT_EXPORT(TRACING_CPP) ObjectArrayInstance {
  ObjectArrayInstance(uint64_t object_id,
                      uint64_t class_id,
                      uint32_t temp_data_position,
                      uint32_t temp_data_length,
                      uint64_t size);

  Instance base_instance;
  const uint64_t class_id;

  // When resolving references from one reference to another, need to reset
  // the hprof offset to temp_data_position and read in the object_ids located
  // at this specific location within the buffer. The parser will make a note of
  // a reference from each instance with given object_id to the current
  // ObjectArrayInstance.
  const uint32_t temp_data_position;

  // The length of the array of object_ids at temp_data_position.
  const uint32_t temp_data_length;
};

// An instance that is an array of primitive objects.
struct COMPONENT_EXPORT(TRACING_CPP) PrimitiveArrayInstance {
  PrimitiveArrayInstance(uint64_t object_id,
                         DataType type,
                         const std::string& type_name,
                         uint64_t size);
  Instance base_instance;
  const DataType type;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_JAVA_HEAP_PROFILER_HPROF_INSTANCES_ANDROID_H_
