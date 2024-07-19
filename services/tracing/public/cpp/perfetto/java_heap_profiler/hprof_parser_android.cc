// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_parser_android.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>

#include "base/android/java_heap_dump_generator.h"
#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_data_type_android.h"
#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_instances_android.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"

// The parsing logic in this file is adapted from Ahat parser for Java heap
// dumps in AOSP's art library:
// art/tools/ahat/src/main/com/android/ahat/heapdump/Parser.java. There is no
// good documentation on the Hprof format other than this parser.

namespace tracing {

namespace {

// Values of tag for instance types in Hprof format.
enum class HprofInstanceTypeId : uint32_t {
  STRING = 0x01,
  CLASS = 0X02,
  HEAP_DUMP = 0x0C,
  HEAP_DUMP_SEGMENT = 0x1C
};

static DataType GetTypeFromIndex(uint32_t type_index) {
  return static_cast<DataType>(type_index);
}

static std::string GenerateArrayIndexString(const std::string& type_name,
                                            uint32_t index) {
  return type_name + "$" + base::NumberToString(index);
}

}  // namespace

const std::string& HprofParser::StringReference::GetString() {
  if (!cached_copy) {
    cached_copy = std::make_unique<std::string>(string_position, length);
  }
  return *cached_copy;
}

HprofParser::HprofParser(const std::string& fp) : file_path_(fp) {}

HprofParser::~HprofParser() = default;
HprofParser::ParseStats::ParseStats() = default;

HprofParser::StringReference::StringReference(const char* string_position,
                                              size_t length)
    : string_position(string_position), length(length) {}

HprofParser::StringReference::~StringReference() = default;

HprofParser::ParseResult HprofParser::ParseStringTag(uint32_t record_length_) {
  parse_stats_.num_strings++;
  ObjectId string_id = hprof_buffer_->GetId();
  DCHECK_GT(record_length_, hprof_buffer_->object_id_size_in_bytes());
  uint32_t string_length =
      record_length_ - hprof_buffer_->object_id_size_in_bytes();
  strings_.emplace(string_id,
                   std::make_unique<StringReference>(
                       hprof_buffer_->DataPosition(), string_length));
  hprof_buffer_->Skip(string_length);

  return ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::ParseClassTag() {
  parse_stats_.num_class_objects++;
  hprof_buffer_->Skip(4);
  ObjectId object_id = hprof_buffer_->GetId();
  hprof_buffer_->Skip(4);
  ObjectId class_string_name_id = hprof_buffer_->GetId();

  auto it = strings_.find(class_string_name_id);
  if (it == strings_.end()) {
    parse_stats_.result = ParseResult::STRING_ID_NOT_FOUND;
    return parse_stats_.result;
  }
  const std::string& class_name = it->second->GetString();

  class_objects_.emplace(object_id,
                         std::make_unique<ClassObject>(object_id, class_name));

  return ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::ParseClassObjectDumpSubtag() {
  parse_stats_.num_class_object_dumps++;
  ObjectId object_id = hprof_buffer_->GetId();
  ClassObject* class_obj = class_objects_[object_id].get();

  hprof_buffer_->Skip(4);
  class_obj->super_class_id = hprof_buffer_->GetId();
  hprof_buffer_->SkipId();
  hprof_buffer_->SkipId();
  hprof_buffer_->SkipId();
  hprof_buffer_->SkipId();
  hprof_buffer_->SkipId();

  uint32_t instance_size = hprof_buffer_->GetFourBytes();
  uint32_t constant_pool_size = hprof_buffer_->GetTwoBytes();
  for (uint32_t i = 0; i < constant_pool_size; ++i) {
    hprof_buffer_->Skip(2);
    uint32_t type_index = hprof_buffer_->GetOneByte();
    hprof_buffer_->Skip(hprof_buffer_->SizeOfType(type_index));
  }

  uint32_t num_static_fields = hprof_buffer_->GetTwoBytes();
  uint64_t static_fields_size = 0;
  for (uint32_t i = 0; i < num_static_fields; ++i) {
    auto it = strings_.find(hprof_buffer_->GetId());
    if (it == strings_.end()) {
      parse_stats_.result = ParseResult::STRING_ID_NOT_FOUND;
      return parse_stats_.result;
    }
    const std::string& static_field_name = it->second->GetString();

    uint32_t type_index = hprof_buffer_->GetOneByte();
    DataType type = GetTypeFromIndex(type_index);
    if (type == DataType::OBJECT) {
      object_id = hprof_buffer_->GetId();
    } else {
      object_id = kInvalidObjectId;
      hprof_buffer_->SkipBytesByType(type);
    }
    static_fields_size += hprof_buffer_->SizeOfType(type_index);

    class_obj->static_fields.emplace_back(static_field_name, type, object_id);
  }
  class_obj->base_instance.size = static_fields_size;

  uint32_t num_instance_fields = hprof_buffer_->GetTwoBytes();
  for (uint32_t i = 0; i < num_instance_fields; ++i) {
    auto it = strings_.find(hprof_buffer_->GetId());
    if (it == strings_.end()) {
      parse_stats_.result = ParseResult::STRING_ID_NOT_FOUND;
      return parse_stats_.result;
    }
    const std::string& instance_field_name = it->second->GetString();
    DataType type = GetTypeFromIndex(hprof_buffer_->GetOneByte());

    class_obj->instance_fields.emplace_back(instance_field_name, type,
                                            kInvalidObjectId);
  }

  class_obj->instance_size = instance_size;

  return ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::ParseClassInstanceDumpSubtag() {
  parse_stats_.num_class_instance_dumps++;
  ObjectId object_id = hprof_buffer_->GetId();
  hprof_buffer_->Skip(4);
  ObjectId class_id = hprof_buffer_->GetId();
  uint32_t instance_size = hprof_buffer_->GetFourBytes();

  uint32_t temp_data_position = hprof_buffer_->offset();
  hprof_buffer_->Skip(instance_size);

  class_instances_.emplace(
      object_id,
      std::make_unique<ClassInstance>(object_id, class_id, temp_data_position));

  return ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::ParseObjectArrayDumpSubtag() {
  parse_stats_.num_object_array_dumps++;
  ObjectId object_id = hprof_buffer_->GetId();
  hprof_buffer_->Skip(4);
  uint32_t length = hprof_buffer_->GetFourBytes();
  ObjectId class_id = hprof_buffer_->GetId();

  object_array_instances_.emplace(
      object_id, std::make_unique<ObjectArrayInstance>(
                     object_id, class_id, hprof_buffer_->offset(), length,
                     length * hprof_buffer_->object_id_size_in_bytes()));

  // Skip data inside object array, to read in later.
  hprof_buffer_->Skip(length * hprof_buffer_->object_id_size_in_bytes());

  return ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::ParsePrimitiveArrayDumpSubtag() {
  parse_stats_.num_primitive_array_dumps++;
  ObjectId object_id = hprof_buffer_->GetId();
  hprof_buffer_->Skip(4);
  uint32_t length = hprof_buffer_->GetFourBytes();

  uint32_t type_index = hprof_buffer_->GetOneByte();
  DataType type = GetTypeFromIndex(type_index);
  uint64_t size = hprof_buffer_->SizeOfType(type_index) * length;

  primitive_array_instances_.emplace(
      object_id, std::make_unique<PrimitiveArrayInstance>(
                     object_id, type, GetTypeString(type_index), size));

  // Don't read in data inside primitive array.
  hprof_buffer_->Skip(size);

  return ParseResult::PARSE_SUCCESS;
}

void HprofParser::ResolveSuperClassFields() {
  // If we have classes A -> B -> C where "->" indicates "super-class of", The
  // members of class C should include members of B and A in order. This method
  // finds all the super classes of each class and includes all their members.

  // Keep track of classes for which we have already included the fields from
  // its super classes.
  std::unordered_set<ObjectId> classes_with_super_fields;
  for (const auto& it : class_objects_) {
    ClassObject* current = it.second.get();
    ClassObject* super = FindClassObject(current->super_class_id);
    for (; super != nullptr; super = FindClassObject(super->super_class_id)) {
      // Always insert at the end.
      current->instance_fields.insert(current->instance_fields.end(),
                                      super->instance_fields.begin(),
                                      super->instance_fields.end());
      // If the current super class has already been resolved to include its
      // super class members, then stop going to further roots since we have all
      // the members needed.
      if (classes_with_super_fields.count(super->base_instance.object_id) > 0)
        break;
    }
    classes_with_super_fields.insert(it.first);
  }
}

struct HprofParser::RegisteredNativeSize {
  raw_ptr<Instance> referent = nullptr;
  uint64_t native_size = 0;
};

HprofParser::RegisteredNativeSize HprofParser::GetRegisteredNativeSize(
    ClassInstance* cleaner_instance) {
  DCHECK_EQ(cleaner_instance->base_instance.type_name, "sun.misc.Cleaner");
  RegisteredNativeSize info;
  if (!SeekToFieldPosition(cleaner_instance, "thunk"))
    return info;
  ClassInstance* thunk_inst = FindClassInstance(hprof_buffer_->GetId());
  if (!SeekToFieldPosition(cleaner_instance, "referent"))
    return info;
  info.referent = FindInstance(hprof_buffer_->GetId());

  if (!thunk_inst || !info.referent)
    return info;

  if (!SeekToFieldPosition(thunk_inst, "this$0"))
    return info;
  ClassInstance* registry_inst = FindClassInstance(hprof_buffer_->GetId());
  if (!registry_inst)
    return info;

  if (!SeekToFieldPosition(registry_inst, "size"))
    return info;
  info.native_size = static_cast<uint64_t>(
      hprof_buffer_->GetUInt64FromBytes(kTypeSizes[DataType::LONG]));
  return info;
}

HprofParser::ParseResult HprofParser::ResolveClassInstanceReferences() {
  for (auto& it : class_instances_) {
    ClassInstance* class_instance = it.second.get();

    ClassObject* class_obj = FindClassObject(class_instance->class_id);
    if (class_obj == nullptr) {
      parse_stats_.result = ParseResult::OBJECT_ID_NOT_FOUND;
      return parse_stats_.result;
    }

    class_instance->base_instance.size = class_obj->instance_size;
    class_instance->base_instance.type_name =
        class_obj->base_instance.type_name;

    hprof_buffer_->set_position(class_instance->temp_data_position);

    for (Field f : class_obj->instance_fields) {
      if (f.type != DataType::OBJECT) {
        hprof_buffer_->SkipBytesByType(f.type);
        continue;
      }

      ObjectId id = hprof_buffer_->GetId();
      Instance* referred_instance = FindInstance(id);

      // If |referred_instance| is not found, just move on to the next id
      // without adding any references.
      if (!referred_instance)
        continue;

      referred_instance->AddReferenceFrom(
          f.name, class_instance->base_instance.object_id);
      class_instance->base_instance.AddReferenceTo(f.name, id);
    }
  }
  return ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::ComputeNativeSizeOfObjects() {
  for (auto& main_class_it : class_instances_) {
    ClassInstance* class_instance = main_class_it.second.get();

    if (class_instance->base_instance.type_name != "sun.misc.Cleaner")
      continue;

    RegisteredNativeSize info = GetRegisteredNativeSize(class_instance);
    if (info.referent) {
      info.referent->size += info.native_size;
    }
  }
  return ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::ResolveObjectArrayInstanceReferences() {
  for (auto& it : object_array_instances_) {
    ObjectArrayInstance* object_array_instance = it.second.get();
    hprof_buffer_->set_position(object_array_instance->temp_data_position);

    auto class_objects_it =
        class_objects_.find(object_array_instance->class_id);
    if (class_objects_it == class_objects_.end()) {
      parse_stats_.result = ParseResult::OBJECT_ID_NOT_FOUND;
      return parse_stats_.result;
    }
    ClassObject* class_obj = class_objects_it->second.get();

    object_array_instance->base_instance.type_name =
        class_obj->base_instance.type_name;

    for (uint32_t i = 0; i < object_array_instance->temp_data_length; i++) {
      ObjectId id = hprof_buffer_->GetId();
      Instance* base_instance = FindInstance(id);

      // If instance is not found, just move on to the next id without adding
      // any references.
      if (!base_instance)
        continue;

      std::string index_str = GenerateArrayIndexString(
          object_array_instance->base_instance.type_name, i);
      base_instance->AddReferenceFrom(
          index_str, object_array_instance->base_instance.object_id);
      object_array_instance->base_instance.AddReferenceTo(index_str, id);
    }
  }
  return ParseResult::PARSE_SUCCESS;
}

void HprofParser::ModifyClassObjectTypeNames() {
  for (auto& it : class_objects_) {
    std::string new_type_name =
        "java.lang.Class:" + it.second.get()->base_instance.type_name;
    it.second.get()->base_instance.type_name = new_type_name;
  }
}

HprofParser::ParseResult HprofParser::ParseHeapDumpTag(
    uint32_t record_length_) {
  parse_stats_.num_heap_dump_segments++;
  size_t end_of_record = hprof_buffer_->offset() + record_length_;
  while (hprof_buffer_->offset() < end_of_record) {
    uint32_t subtag = hprof_buffer_->GetOneByte();
    switch (subtag) {
      case 0x01: {  // ROOT_JNI_GLOBAL = 1;
        ObjectId root_object_id = hprof_buffer_->GetId();
        hprof_buffer_->SkipId();
        roots_[HeapGraphRoot::Type::ROOT_JNI_GLOBAL].push_back(root_object_id);
        break;
      }
      case 0x02: {  // ROOT_JNI_LOCAL = 2;
        ObjectId root_object_id = hprof_buffer_->GetId();
        hprof_buffer_->Skip(8);
        roots_[HeapGraphRoot::Type::ROOT_JNI_LOCAL].push_back(root_object_id);
        break;
      }
      case 0x03: {  // ROOT_JAVA_FRAME = 3;
        ObjectId root_object_id = hprof_buffer_->GetId();
        hprof_buffer_->Skip(8);
        roots_[HeapGraphRoot::Type::ROOT_JAVA_FRAME].push_back(root_object_id);
        break;
      }
      case 0x04: {  // ROOT_NATIVE_STACK = 4;
        ObjectId root_object_id = hprof_buffer_->GetId();
        hprof_buffer_->Skip(4);
        roots_[HeapGraphRoot::Type::ROOT_NATIVE_STACK].push_back(
            root_object_id);
        break;
      }
      case 0x05: {  // ROOT_STICKY_CLASS = 5;

        ObjectId root_object_id = hprof_buffer_->GetId();
        roots_[HeapGraphRoot::Type::ROOT_STICKY_CLASS].push_back(
            root_object_id);
        break;
      }
      case 0x06: {  // ROOT_THREAD_BLOCK = 6;

        ObjectId root_object_id = hprof_buffer_->GetId();
        hprof_buffer_->Skip(4);
        roots_[HeapGraphRoot::Type::ROOT_THREAD_BLOCK].push_back(
            root_object_id);
        break;
      }
      case 0x07: {  // ROOT_MONITOR_USED = 7;
        ObjectId root_object_id = hprof_buffer_->GetId();
        roots_[HeapGraphRoot::Type::ROOT_MONITOR_USED].push_back(
            root_object_id);
        break;
      }
      case 0x08: {  // ROOT_THREAD_OBJECT = 8;
        ObjectId root_object_id = hprof_buffer_->GetId();
        hprof_buffer_->Skip(8);
        roots_[HeapGraphRoot::Type::ROOT_THREAD_OBJECT].push_back(
            root_object_id);
        break;
      }
      case 0x89: {  // ROOT_INTERNED_STRING = 9
        ObjectId root_object_id = hprof_buffer_->GetId();
        roots_[HeapGraphRoot::Type::ROOT_INTERNED_STRING].push_back(
            root_object_id);
        break;
      }
      case 0x8a: {  // ROOT_FINALIZING = 10;
        ObjectId root_object_id = hprof_buffer_->GetId();
        roots_[HeapGraphRoot::Type::ROOT_FINALIZING].push_back(root_object_id);
        break;
      }
      case 0x8b: {  // ROOT_DEBUGGER = 11;
        ObjectId root_object_id = hprof_buffer_->GetId();
        roots_[HeapGraphRoot::Type::ROOT_DEBUGGER].push_back(root_object_id);
        break;
      }
      case 0x8d: {  // ROOT_VM_INTERNAL = 13;
        ObjectId root_object_id = hprof_buffer_->GetId();
        roots_[HeapGraphRoot::Type::ROOT_VM_INTERNAL].push_back(root_object_id);
        break;
      }
      case 0x8e: {  // ROOT_JNI_MONITOR = 14;
        ObjectId root_object_id = hprof_buffer_->GetId();
        hprof_buffer_->Skip(8);
        roots_[HeapGraphRoot::Type::ROOT_JNI_MONITOR].push_back(root_object_id);
        break;
      }
      case 0xfe:  // HEAP DUMP INFO (ANDROID)
        hprof_buffer_->Skip(hprof_buffer_->object_id_size_in_bytes() + 4);
        break;
      case 0xff: {  // ROOT_UNKNOWN = 0;
        ObjectId root_object_id = hprof_buffer_->GetId();
        roots_[HeapGraphRoot::Type::ROOT_UNKNOWN].push_back(root_object_id);
        break;
      }

      case 0x20: {  // CLASS DUMP
        ParseResult result = ParseClassObjectDumpSubtag();
        if (result != ParseResult::PARSE_SUCCESS) {
          return result;
        };
        break;
      }

      case 0x21: {  // CLASS INSTANCE DUMP
        ParseResult result = ParseClassInstanceDumpSubtag();
        if (result != ParseResult::PARSE_SUCCESS) {
          return result;
        };
        break;
      }

      case 0x22: {  // OBJECT ARRAY DUMP
        ParseResult result = ParseObjectArrayDumpSubtag();
        if (result != ParseResult::PARSE_SUCCESS) {
          return result;
        };
        break;
      }

      case 0x23: {  // PRIMITIVE ARRAY DUMP
        ParseResult result = ParsePrimitiveArrayDumpSubtag();
        if (result != ParseResult::PARSE_SUCCESS) {
          return result;
        };
        break;
      }

      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  return ParseResult::PARSE_SUCCESS;
}

void HprofParser::ParseFileData(const unsigned char* file_data,
                                size_t file_size) {
  hprof_buffer_ = std::make_unique<HprofBuffer>(file_data, file_size);

  uint32_t id_size;
  // Skip all leading 0s until we find the |id_size|.
  while (hprof_buffer_->GetOneByte() != 0 && hprof_buffer_->HasRemaining()) {
  }
  id_size = hprof_buffer_->GetFourBytes();
  hprof_buffer_->set_id_size(id_size);

  hprof_buffer_->Skip(4);  // hightime
  hprof_buffer_->Skip(4);  // lowtime

  while (hprof_buffer_->HasRemaining()) {
    HprofInstanceTypeId tag =
        static_cast<HprofInstanceTypeId>(hprof_buffer_->GetOneByte());
    hprof_buffer_->Skip(4);  // time
    uint32_t record_length_ = hprof_buffer_->GetFourBytes();

    switch (tag) {
      case HprofInstanceTypeId::STRING: {
        if (ParseStringTag(record_length_) != ParseResult::PARSE_SUCCESS) {
          return;
        };
        break;
      }

      case HprofInstanceTypeId::CLASS: {
        if (ParseClassTag() != ParseResult::PARSE_SUCCESS) {
          return;
        };
        break;
      }

      // TODO(zhanggeorge): Test this tag match.
      case HprofInstanceTypeId::HEAP_DUMP:
      case HprofInstanceTypeId::HEAP_DUMP_SEGMENT: {
        if (ParseHeapDumpTag(record_length_) != ParseResult::PARSE_SUCCESS) {
          return;
        }
        break;
      }
      default:
        hprof_buffer_->Skip(record_length_);
    }
  }

  // Currently we have all instances defined in the hprof file within four
  // separate id->instance based off instance type (ClassObject, ClassInstance,
  // ObjectArrayInstance, PrimitiveArrayInstance). The next step is to take
  // these instances and resolve references between pairs of instances.
  // We do this specifically for ClassInstances and ObjectArrayInstances.
  // For ClassInstances, we set a reference between a given class instance
  // and any instances that are instance variables of the given class instance.
  // For ObjectArrayInstances, we set a reference between objects within the
  // object array and the actual object array.
  ResolveSuperClassFields();

  if (ResolveClassInstanceReferences() != ParseResult::PARSE_SUCCESS) {
    return;
  }

  if (ComputeNativeSizeOfObjects() != ParseResult::PARSE_SUCCESS) {
    return;
  }

  ParseResult object_array_instance_result =
      ResolveObjectArrayInstanceReferences();
  if (object_array_instance_result != ParseResult::PARSE_SUCCESS) {
    return;
  }

  ModifyClassObjectTypeNames();

  parse_stats_.result = ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::Parse() {
  base::ScopedFD fd(open(file_path_.c_str(), O_RDONLY));
  if (!fd.is_valid()) {
    parse_stats_.result = HprofParser::ParseResult::FAILED_TO_OPEN_FILE;
    return parse_stats_.result;
  }

  struct stat file_stats;
  if (stat(file_path_.c_str(), &file_stats) < 0) {
    parse_stats_.result = HprofParser::ParseResult::FAILED_TO_OPEN_FILE;
    return parse_stats_.result;
  }

  void* file_data =
      mmap(0, file_stats.st_size, PROT_READ, MAP_PRIVATE, fd.get(), 0);
  if (file_data == MAP_FAILED) {
    parse_stats_.result = HprofParser::ParseResult::FAILED_TO_OPEN_FILE;
    return parse_stats_.result;
  }

  ParseFileData(reinterpret_cast<const unsigned char*>(file_data),
                file_stats.st_size);

  int res = munmap(file_data, file_stats.st_size);
  DCHECK_EQ(res, 0);

  return parse_stats_.result;
}

ClassInstance* HprofParser::FindClassInstance(ObjectId id) {
  auto class_instance_sub_instance_it = class_instances_.find(id);
  if (class_instance_sub_instance_it != class_instances_.end()) {
    return class_instance_sub_instance_it->second.get();
  }
  return nullptr;
}
ClassObject* HprofParser::FindClassObject(ObjectId id) {
  auto class_object_sub_instance_it = class_objects_.find(id);
  if (class_object_sub_instance_it != class_objects_.end()) {
    return class_object_sub_instance_it->second.get();
  }
  return nullptr;
}

Instance* HprofParser::FindInstance(ObjectId id) {
  auto class_instance_sub_instance_it = class_instances_.find(id);
  if (class_instance_sub_instance_it != class_instances_.end()) {
    return &class_instance_sub_instance_it->second->base_instance;
  }

  auto object_array_sub_instance_it = object_array_instances_.find(id);
  if (object_array_sub_instance_it != object_array_instances_.end()) {
    return &object_array_sub_instance_it->second->base_instance;
  }

  auto primitive_array_sub_instance_it = primitive_array_instances_.find(id);
  if (primitive_array_sub_instance_it != primitive_array_instances_.end()) {
    return &primitive_array_sub_instance_it->second->base_instance;
  }

  auto class_object_sub_instance_it = class_objects_.find(id);
  if (class_object_sub_instance_it != class_objects_.end()) {
    return &class_object_sub_instance_it->second->base_instance;
  }

  return nullptr;
}

bool HprofParser::SeekToFieldPosition(ClassInstance* instance,
                                      std::string_view field_name) {
  ClassObject* class_obj = FindClassObject(instance->class_id);
  if (class_obj == nullptr)
    return false;

  hprof_buffer_->set_position(instance->temp_data_position);
  for (Field f : class_obj->instance_fields) {
    if (field_name == f.name)
      return true;
    hprof_buffer_->SkipBytesByType(f.type);
  }
  return false;
}

}  // namespace tracing
