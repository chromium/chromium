// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_parser_android.h"

#include <string_view>

#include "base/android/java_heap_dump_generator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_buffer_android.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

namespace {

uint32_t ToLittleEndian(uint32_t val) {
  uint32_t res = 0;
  for (unsigned i = 0; i < sizeof(val); ++i) {
    res |= (val & 0xFF) << (8 * (sizeof(val) - i - 1));
    val = val >> 8;
  }
  return res;
}

}  // namespace

TEST(HprofParserTest, BasicParse) {
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    VLOG(0) << "Failed to create unique temporary directory.";
    return;
  }
  const std::string file_path_str =
      temp_dir.GetPath().Append("temp_hprof.hprof").value();

  base::android::WriteJavaHeapDumpToPath(file_path_str);
  HprofParser parser(file_path_str);
  parser.Parse();

  static const uint64_t kObjectCountThreshold = 100;
  EXPECT_GT(parser.parse_stats().num_strings, kObjectCountThreshold);
  EXPECT_GT(parser.parse_stats().num_class_objects, kObjectCountThreshold);
  EXPECT_GT(parser.parse_stats().num_heap_dump_segments, kObjectCountThreshold);
  EXPECT_GT(parser.parse_stats().num_class_object_dumps, kObjectCountThreshold);
  EXPECT_GT(parser.parse_stats().num_class_instance_dumps,
            kObjectCountThreshold);
  EXPECT_GT(parser.parse_stats().num_object_array_dumps, kObjectCountThreshold);
  EXPECT_GT(parser.parse_stats().num_primitive_array_dumps,
            kObjectCountThreshold);

  EXPECT_EQ(parser.parse_stats().result,
            HprofParser::ParseResult::PARSE_SUCCESS);
}

TEST(HprofParserTest, InvalidPathWithNoDump) {
  HprofParser parser("invalid_file");
  parser.Parse();
  EXPECT_EQ(parser.parse_stats().result,
            HprofParser::ParseResult::FAILED_TO_OPEN_FILE);
}

TEST(HprofParserTest, ParseStringTag) {
  const int length = 8;
  unsigned char file_data[length]{0,   0,   0,   1,     // string_id
                                  116, 101, 115, 116};  // "test"
  HprofParser parser("dummy_file");
  parser.hprof_buffer_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);
  parser.ParseStringTag(8);

  EXPECT_NE(parser.strings_.find(1), parser.strings_.end());
  EXPECT_EQ(parser.strings_[1]->GetString(), "test");
}

TEST(HprofParserTest, ParseClassTag) {
  const int length = 20;
  unsigned char file_data[length]{0,   0,   0,   0,  0, 0, 0, 2,  // object_id
                                  0,   0,   0,   0,  0, 0, 0, 1,  // string_id
                                  116, 101, 115, 116};            // "test"
  HprofParser parser("dummy_file");
  parser.hprof_buffer_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  parser.strings_.emplace(
      1, std::make_unique<HprofParser::StringReference>(
             reinterpret_cast<const char*>(file_data + 16), 4));

  parser.ParseClassTag();
  auto it = parser.class_objects_.find(2);

  EXPECT_NE(it, parser.class_objects_.end());
  EXPECT_EQ(it->second->base_instance.type_name, "test");
  EXPECT_EQ(it->second->base_instance.object_id, 2u);
}

TEST(HprofParserTest, ParseClassObjectDumpSubtag) {
  const int length = 64;
  unsigned char file_data[length]{
      0,   0,   0,   3,  // object_id
      0,   0,   0,   0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,   0,   0,   0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8,  // instance_size
      0,   1,  // constant_pool_size
      0,   0,
      4,                  // type_index
      0,   0,   1,        // num_static_fields
      0,   0,   0,   1,   // string_id
      2,                  // type_index
      0,   0,   0,   2,   // object_id
      0,   1,             // num_instance_fields
      0,   0,   0,   1,   // string_id
      4,                  // type_index
      116, 101, 115, 116  // "test"
  };
  HprofParser parser("dummy_file");
  parser.hprof_buffer_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  parser.strings_.emplace(
      1, std::make_unique<HprofParser::StringReference>(
             reinterpret_cast<const char*>(file_data + 60), 4));

  parser.class_objects_.emplace(
      3, std::make_unique<ClassObject>(3, "class_obj_dummy"));

  parser.ParseClassObjectDumpSubtag();
  auto it = parser.class_objects_.find(3);

  EXPECT_NE(it, parser.class_objects_.end());

  Field static_dummy_field = it->second->static_fields.back();
  EXPECT_EQ(static_dummy_field.name, "test");
  EXPECT_EQ(static_dummy_field.type, DataType::OBJECT);
  EXPECT_EQ(static_dummy_field.object_id, 2u);

  Field instance_dummy_field = it->second->instance_fields.back();
  EXPECT_EQ(instance_dummy_field.name, "test");
  EXPECT_EQ(instance_dummy_field.type, DataType::BOOLEAN);
  EXPECT_EQ(instance_dummy_field.object_id, kInvalidObjectId);

  EXPECT_EQ(it->second->base_instance.size, 4u);
  EXPECT_EQ(it->second->instance_size, 8u);
}

TEST(HprofParserTest, ParseClassInstanceDumpSubtag) {
  const int length = 20;
  unsigned char file_data[length]{0, 0, 0, 2,              // object_id
                                  0, 0, 0, 0, 0, 0, 0, 7,  // class_id
                                  0, 0, 0, 4,              // instance_size
                                  0, 0, 0, 0};
  HprofParser parser("dummy_file");
  parser.hprof_buffer_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  parser.ParseClassInstanceDumpSubtag();
  auto it = parser.class_instances_.find(2);

  EXPECT_NE(it, parser.class_instances_.end());
  EXPECT_EQ(it->second->class_id, 7u);
  EXPECT_EQ(it->second->base_instance.object_id, 2u);
  EXPECT_EQ(it->second->temp_data_position, 16u);
}

TEST(HprofParserTest, ParseObjectArrayDumpSubtag) {
  const int length = 24;
  unsigned char file_data[length]{0, 0, 0, 2,              // object_id
                                  0, 0, 0, 0, 0, 0, 0, 2,  // length
                                  0, 0, 0, 4,              // class_id
                                  0, 0, 0, 0, 0, 0, 0, 0};
  HprofParser parser("dummy_file");
  parser.hprof_buffer_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  parser.ParseObjectArrayDumpSubtag();
  auto it = parser.object_array_instances_.find(2);

  EXPECT_NE(it, parser.object_array_instances_.end());
  EXPECT_EQ(it->second->class_id, 4u);
  EXPECT_EQ(it->second->base_instance.object_id, 2u);
  EXPECT_EQ(it->second->temp_data_position, 16u);
  EXPECT_EQ(it->second->temp_data_length, 2u);
  EXPECT_EQ(it->second->base_instance.size, 8u);
}

TEST(HprofParserTest, ParsePrimitiveArrayDumpSubtag) {
  const int length = 17;
  unsigned char file_data[length]{0, 0, 0, 2,              // object_id
                                  0, 0, 0, 0, 0, 0, 0, 4,  // length
                                  4,                       // type_index
                                  0, 0, 0, 0};
  HprofParser parser("dummy_file");
  parser.hprof_buffer_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  parser.ParsePrimitiveArrayDumpSubtag();
  auto it = parser.primitive_array_instances_.find(2);

  EXPECT_NE(it, parser.primitive_array_instances_.end());
  EXPECT_EQ(it->second->base_instance.object_id, 2u);
  EXPECT_EQ(it->second->type, DataType::BOOLEAN);
  EXPECT_EQ(it->second->base_instance.type_name, "bool[]");
  EXPECT_EQ(it->second->base_instance.size, 4u);
}

TEST(HprofParserTest, BasicResolveClassInstanceReferences) {
  const int length = 8;
  unsigned char file_data[length]{
      0, 0, 0, 101,  // instance_field_1
      0, 0, 0, 102   // instance_field_2
  };
  HprofParser parser("dummy_file");
  parser.hprof_buffer_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  // Class object with 1 instance field.
  parser.class_objects_.emplace(
      3, std::make_unique<ClassObject>(3, "class_obj_dummy"));
  parser.class_objects_[3].get()->instance_size = 40;
  parser.class_objects_[3].get()->instance_fields.emplace_back(
      Field("instance_field_1", DataType::OBJECT, 3));
  parser.class_objects_[3].get()->instance_fields.emplace_back(
      Field("instance_field_2", DataType::OBJECT, 3));

  // Class object with zero instance fields.
  parser.class_objects_.emplace(
      4, std::make_unique<ClassObject>(4, "class_obj_dummy_empty"));
  parser.class_objects_[4].get()->instance_size = 44;

  // Parent Instance of type class_obj_dummy.
  parser.class_instances_.emplace(100,
                                  std::make_unique<ClassInstance>(100, 3, 0));

  // Sub Instance of Parent Instance of type class_obj_dummy_empty.
  parser.class_instances_.emplace(101,
                                  std::make_unique<ClassInstance>(101, 4, 0));

  // Another Sub Instance of Parent Instance of type class_obj_dummy_empty.
  parser.class_instances_.emplace(102,
                                  std::make_unique<ClassInstance>(102, 4, 0));

  parser.ResolveClassInstanceReferences();

  auto parent_it = parser.class_instances_.find(100);
  EXPECT_EQ(parent_it->second->base_instance.size, 40u);
  EXPECT_EQ(parent_it->second->base_instance.type_name, "class_obj_dummy");
  ASSERT_EQ(parent_it->second->base_instance.referred_to.size(), 2u);

  auto sub_it_1 = parser.class_instances_.find(101);
  EXPECT_EQ(sub_it_1->second->base_instance.size, 44u);
  EXPECT_EQ(sub_it_1->second->base_instance.referred_by.size(), 1u);
  EXPECT_EQ(sub_it_1->second->base_instance.referred_by.back().referred_by_name,
            "instance_field_1");
  EXPECT_EQ(sub_it_1->second->base_instance.referred_by.back()
                .referred_from_object_id,
            100u);
  EXPECT_EQ(parent_it->second->base_instance.referred_to[0].referred_by_name,
            "instance_field_1");
  EXPECT_EQ(
      parent_it->second->base_instance.referred_to[0].referred_from_object_id,
      101u);

  auto sub_it_2 = parser.class_instances_.find(102);
  EXPECT_EQ(sub_it_2->second->base_instance.size, 44u);
  EXPECT_EQ(sub_it_2->second->base_instance.referred_by.size(), 1u);
  EXPECT_EQ(sub_it_2->second->base_instance.referred_by.back().referred_by_name,
            "instance_field_2");
  EXPECT_EQ(sub_it_2->second->base_instance.referred_by.back()
                .referred_from_object_id,
            100u);
  EXPECT_EQ(parent_it->second->base_instance.referred_to[1].referred_by_name,
            "instance_field_2");
  EXPECT_EQ(
      parent_it->second->base_instance.referred_to[1].referred_from_object_id,
      102u);
}

std::unique_ptr<ClassObject> GetClassObjectWith2Fields(int id,
                                                       std::string_view name) {
  auto obj = std::make_unique<ClassObject>(id, std::string{name});
  obj->instance_fields = {Field(base::StrCat({name, "_f1"}), DataType::INT, 0),
                          Field(base::StrCat({name, "_f2"}), DataType::INT, 0)};
  return obj;
}

TEST(HprofParserTest, ResolveSuperClassObjectFields) {
  HprofParser parser("dummy_file");
  parser.class_objects_.emplace(1, GetClassObjectWith2Fields(1, "base"));
  parser.class_objects_.emplace(2, GetClassObjectWith2Fields(2, "child1"));
  parser.class_objects_[2]->super_class_id = 1;
  parser.class_objects_.emplace(3, GetClassObjectWith2Fields(3, "child2"));
  parser.class_objects_[3]->super_class_id = 1;
  parser.class_objects_.emplace(4, GetClassObjectWith2Fields(4, "child11"));
  parser.class_objects_[4]->super_class_id = 2;
  parser.class_objects_.emplace(5, GetClassObjectWith2Fields(5, "child12"));
  parser.class_objects_[5]->super_class_id = 2;
  parser.ResolveSuperClassFields();

  std::vector<const char*> expected[] = {
      {"base", "base_f1", "base_f2"},
      {"child1", "child1_f1", "child1_f2", "base_f1", "base_f2"},
      {"child2", "child2_f1", "child2_f2", "base_f1", "base_f2"},
      {"child11", "child11_f1", "child11_f2", "child1_f1", "child1_f2",
       "base_f1", "base_f2"},
      {"child12", "child12_f1", "child12_f2", "child1_f1", "child1_f2",
       "base_f1", "base_f2"},
  };

  ASSERT_EQ(parser.class_objects().size(), std::size(expected));
  for (unsigned i = 1; i < std::size(expected); ++i) {
    const auto& expected_class = expected[i - 1];
    auto it = parser.class_objects().find(i);
    ASSERT_TRUE(it != parser.class_objects().end())
        << "missing object id " << i;
    const auto& processed = *it->second;
    EXPECT_EQ(processed.base_instance.type_name, expected_class[0]);
    ASSERT_EQ(processed.instance_fields.size(), expected_class.size() - 1);
    for (unsigned field_num = 0; field_num < processed.instance_fields.size();
         ++field_num) {
      EXPECT_EQ(processed.instance_fields[field_num].name,
                expected_class[field_num + 1]);
    }
  }
}

TEST(HprofParserTest, MissingObjectReferenceResolveClassInstanceReferences) {
  const int length = 8;
  unsigned char file_data[length]{
      0, 0, 0, 101,  // instance_field_1
      0, 0, 0, 102   // instance_field_2
  };
  HprofParser parser("dummy_file");
  parser.hprof_buffer_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  // Class object with 1 instance field.
  parser.class_objects_.emplace(
      3, std::make_unique<ClassObject>(3, "class_obj_dummy"));
  parser.class_objects_[3].get()->instance_size = 40;
  parser.class_objects_[3].get()->instance_fields.emplace_back(
      Field("instance_field_1", DataType::OBJECT, 3));
  parser.class_objects_[3].get()->instance_fields.emplace_back(
      Field("instance_field_2", DataType::OBJECT, 3));

  // Class object with zero instance fields.
  parser.class_objects_.emplace(
      4, std::make_unique<ClassObject>(4, "class_obj_dummy_empty"));
  parser.class_objects_[4].get()->instance_size = 44;

  // Parent Instance of type class_obj_dummy.
  parser.class_instances_.emplace(100,
                                  std::make_unique<ClassInstance>(100, 3, 0));

  // Even though the two instance fields do not exist, this should not error
  // and the original ClassObject should not have been modified.
  parser.ResolveClassInstanceReferences();

  auto parent_it = parser.class_instances_.find(100);
  EXPECT_EQ(parent_it->second->base_instance.size, 40u);
  EXPECT_EQ(parent_it->second->base_instance.type_name, "class_obj_dummy");
}

TEST(HprofParserTest,
     ExistingAndMissingReferencesResolveClassInstanceReferences) {
  const int length = 8;
  unsigned char file_data[length]{
      0, 0, 0, 101,  // instance_field_1
      0, 0, 0, 102   // instance_field_2
  };
  HprofParser parser("dummy_file");
  parser.hprof_buffer_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  // Class object with 1 instance field.
  parser.class_objects_.emplace(
      3, std::make_unique<ClassObject>(3, "class_obj_dummy"));
  parser.class_objects_[3].get()->instance_size = 40;
  parser.class_objects_[3].get()->instance_fields.emplace_back(
      Field("instance_field_1", DataType::OBJECT, 3));
  parser.class_objects_[3].get()->instance_fields.emplace_back(
      Field("instance_field_2", DataType::OBJECT, 3));

  // Class object with zero instance fields.
  parser.class_objects_.emplace(
      4, std::make_unique<ClassObject>(4, "class_obj_dummy_empty"));
  parser.class_objects_[4].get()->instance_size = 44;

  // Parent Instance of type class_obj_dummy.
  parser.class_instances_.emplace(100,
                                  std::make_unique<ClassInstance>(100, 3, 0));

  // Another Sub Instance of Parent Instance of type class_obj_dummy_empty.
  parser.class_instances_.emplace(102,
                                  std::make_unique<ClassInstance>(102, 4, 0));

  // Even though ClassInstance with object_id 101 (instance_field_1) does not
  // exist, the reference
  // between 100 and 102 should still be resolved without an error.
  parser.ResolveClassInstanceReferences();

  auto parent_it = parser.class_instances_.find(100);
  EXPECT_EQ(parent_it->second->base_instance.size, 40u);
  EXPECT_EQ(parent_it->second->base_instance.type_name, "class_obj_dummy");

  auto sub_it_2 = parser.class_instances_.find(102);
  EXPECT_EQ(sub_it_2->second->base_instance.size, 44u);
  EXPECT_EQ(sub_it_2->second->base_instance.referred_by.size(), 1u);
  EXPECT_EQ(sub_it_2->second->base_instance.referred_by.back().referred_by_name,
            "instance_field_2");
  EXPECT_EQ(sub_it_2->second->base_instance.referred_by.back()
                .referred_from_object_id,
            100u);
}

TEST(HprofParserTest, MultipleInstanceFieldsResolveClassInstanceReferences) {
  const int length = 8;
  unsigned char file_data[length]{
      0, 0, 0, 101,  // object array instance
      0, 0, 0, 102   // primitive array instance
  };
  HprofParser parser("dummy_file");
  parser.hprof_buffer_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  // Class object with 1 instance field.
  parser.class_objects_.emplace(
      3, std::make_unique<ClassObject>(3, "class_obj_dummy"));
  parser.class_objects_[3].get()->instance_size = 40;
  parser.class_objects_[3].get()->instance_fields.emplace_back(
      Field("object_array_instance", DataType::OBJECT, 3));
  parser.class_objects_[3].get()->instance_fields.emplace_back(
      Field("primitive_array_instance", DataType::OBJECT, 3));

  // Class object with zero instance fields.
  parser.class_objects_.emplace(
      4, std::make_unique<ClassObject>(4, "class_obj_dummy_empty"));
  parser.class_objects_[4].get()->instance_size = 44;

  // Parent Instance of type class_obj_dummy.
  parser.class_instances_.emplace(100,
                                  std::make_unique<ClassInstance>(100, 3, 0));

  // Sub Object Arrray Instance of Parent Instance of type
  // class_obj_dummy_empty.
  parser.object_array_instances_.emplace(
      101, std::make_unique<ObjectArrayInstance>(101, 4, 0, 0, 10));

  // Sub Primitive Arrray Instance of Parent Instance of type
  // class_obj_dummy_empty.
  parser.primitive_array_instances_.emplace(
      102, std::make_unique<PrimitiveArrayInstance>(102, DataType::BOOLEAN,
                                                    "bool[]", 20));

  parser.ResolveClassInstanceReferences();

  auto parent_it = parser.class_instances_.find(100);
  EXPECT_EQ(parent_it->second->base_instance.size, 40u);
  EXPECT_EQ(parent_it->second->base_instance.type_name, "class_obj_dummy");

  auto object_sub_it = parser.object_array_instances_.find(101);
  EXPECT_EQ(object_sub_it->second->base_instance.size, 10u);
  EXPECT_EQ(object_sub_it->second->base_instance.referred_by.size(), 1u);
  EXPECT_EQ(
      object_sub_it->second->base_instance.referred_by.back().referred_by_name,
      "object_array_instance");
  EXPECT_EQ(object_sub_it->second->base_instance.referred_by.back()
                .referred_from_object_id,
            100u);

  auto prim_sub_it = parser.primitive_array_instances_.find(102);
  EXPECT_EQ(prim_sub_it->second->base_instance.size, 20u);
  EXPECT_EQ(prim_sub_it->second->base_instance.referred_by.size(), 1u);
  EXPECT_EQ(
      prim_sub_it->second->base_instance.referred_by.back().referred_by_name,
      "primitive_array_instance");
  EXPECT_EQ(prim_sub_it->second->base_instance.referred_by.back()
                .referred_from_object_id,
            100u);
}

TEST(HprofParserTest, BasicResolveObjectArrayInstanceReferences) {
  const int length = 4;
  unsigned char file_data[length]{
      0, 0, 0, 201  // object array member instance
  };
  HprofParser parser("dummy_file");
  parser.hprof_buffer_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  parser.class_objects_.emplace(
      4, std::make_unique<ClassObject>(4, "class_obj_dummy_empty"));
  parser.class_objects_[4].get()->instance_size = 44;

  parser.class_objects_.emplace(
      5, std::make_unique<ClassObject>(5, "java.lang.Object[]"));

  // Object array of type class_obj_dummy_empty. Has length one and contains one
  // object declared below.
  parser.object_array_instances_.emplace(
      200, std::make_unique<ObjectArrayInstance>(200, 5, 0, 1, 4));

  // A member of above object array of type class_obj_dummy_empty.
  parser.class_instances_.emplace(201,
                                  std::make_unique<ClassInstance>(201, 4, 0));

  parser.ResolveObjectArrayInstanceReferences();

  auto object_arr_parent_int = parser.object_array_instances_.find(200);
  EXPECT_EQ(object_arr_parent_int->second->base_instance.size, 4u);
  EXPECT_EQ(object_arr_parent_int->second->base_instance.type_name,
            "java.lang.Object[]");
  ASSERT_EQ(object_arr_parent_int->second->base_instance.referred_to.size(),
            1u);

  auto object_arr_sub_int = parser.class_instances_.find(201);
  ASSERT_EQ(object_arr_sub_int->second->base_instance.referred_by.size(), 1u);
  EXPECT_EQ(object_arr_sub_int->second->base_instance.referred_by.back()
                .referred_by_name,
            "java.lang.Object[]$0");
  EXPECT_EQ(object_arr_sub_int->second->base_instance.referred_by.back()
                .referred_from_object_id,
            200u);
  EXPECT_EQ(object_arr_parent_int->second->base_instance.referred_to.back()
                .referred_by_name,
            "java.lang.Object[]$0");
  EXPECT_EQ(object_arr_parent_int->second->base_instance.referred_to.back()
                .referred_from_object_id,
            201u);
}

TEST(HprofParserTest,
     MissingAndExistingReferencesResolveObjectArrayInstanceReferences) {
  const int length = 8;
  unsigned char file_data[length]{
      0, 0, 0, 201,  // object array member instance
      0, 0, 0, 202   // object array member instance
  };
  HprofParser parser("dummy_file");
  parser.hprof_buffer_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  parser.class_objects_.emplace(
      4, std::make_unique<ClassObject>(4, "class_obj_dummy_empty"));
  parser.class_objects_[4].get()->instance_size = 44;

  parser.class_objects_.emplace(
      5, std::make_unique<ClassObject>(5, "java.lang.Object[]"));

  // Object array of type class_obj_dummy_empty. Has length one and contains one
  // object declared below.
  parser.object_array_instances_.emplace(
      200, std::make_unique<ObjectArrayInstance>(200, 5, 0, 2, 4));

  // A member of above object array of type class_obj_dummy_empty.
  parser.class_instances_.emplace(202,
                                  std::make_unique<ClassInstance>(202, 4, 0));

  // Even though ClassInstance with object_id 201 does not exist, the reference
  // between 200 and 202 should still be resolved without an error.
  parser.ResolveObjectArrayInstanceReferences();

  auto object_arr_parent_int = parser.object_array_instances_.find(200);
  EXPECT_EQ(object_arr_parent_int->second->base_instance.size, 4u);
  EXPECT_EQ(object_arr_parent_int->second->base_instance.type_name,
            "java.lang.Object[]");

  auto object_arr_sub_int = parser.class_instances_.find(202);
  EXPECT_EQ(object_arr_sub_int->second->base_instance.referred_by.size(), 1u);
  EXPECT_EQ(object_arr_sub_int->second->base_instance.referred_by.back()
                .referred_by_name,
            "java.lang.Object[]$1");
  EXPECT_EQ(object_arr_sub_int->second->base_instance.referred_by.back()
                .referred_from_object_id,
            200u);
}

TEST(HprofParserTest, ModifyClassObjectTypeNames) {
  const int length = 0;
  unsigned char file_data[length]{};
  HprofParser parser("dummy_file");
  parser.hprof_buffer_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  parser.class_objects_.emplace(
      3, std::make_unique<ClassObject>(3, "class_obj_dummy"));

  parser.class_objects_.emplace(
      4, std::make_unique<ClassObject>(4, "class_obj_dummy_empty"));

  parser.ModifyClassObjectTypeNames();
  auto class_obj_it = parser.class_objects_.find(3);
  EXPECT_EQ(class_obj_it->second->base_instance.type_name,
            "java.lang.Class:class_obj_dummy");

  auto class_obj__empty_it = parser.class_objects_.find(4);
  EXPECT_EQ(class_obj__empty_it->second->base_instance.type_name,
            "java.lang.Class:class_obj_dummy_empty");
}

TEST(HprofParserTest, NativeSizeComputation) {
  HprofParser parser("dummy_file");
  constexpr uint32_t kCleanrInstanceId = 101;
  constexpr uint32_t kThunkInstanceId = 102;
  constexpr uint32_t kRegistryInstanceId = 103;
  constexpr uint32_t kReferentInstanceId = 104;
  // Sample data that mocks a sun.misc.Cleaner instance.
  uint32_t file_data[]{// Fields of Cleaner instance:
                       ToLittleEndian(kThunkInstanceId), /*dummy=*/0,
                       ToLittleEndian(kReferentInstanceId),
                       // Thunk:
                       ToLittleEndian(kRegistryInstanceId),
                       // Registry
                       0, ToLittleEndian(9876)};
  const int length = std::size(file_data) * sizeof(uint32_t);
  constexpr uint32_t kCleanerOffset = 0;    // First object
  constexpr uint32_t kThunkOffset = 12;     // Second object
  constexpr uint32_t kRegistryOffset = 16;  // Third object

  parser.hprof_buffer_ = std::make_unique<HprofBuffer>(
      reinterpret_cast<const unsigned char*>(file_data), length);

  auto cleaner_inst =
      std::make_unique<ClassInstance>(kCleanrInstanceId, 51, kCleanerOffset);
  auto cleaner_class = std::make_unique<ClassObject>(51, "sun.misc.Cleaner");
  cleaner_class->instance_fields = {Field("thunk", DataType::OBJECT, 0),
                                    Field("dummy", DataType::OBJECT, 0),
                                    Field("referent", DataType::OBJECT, 0)};
  auto thunk_inst =
      std::make_unique<ClassInstance>(kThunkInstanceId, 52, kThunkOffset);
  auto thunk_class = std::make_unique<ClassObject>(52, "Thunk");
  thunk_class->instance_fields = {Field("this$0", DataType::OBJECT, 0)};
  auto registry_inst =
      std::make_unique<ClassInstance>(kRegistryInstanceId, 53, kRegistryOffset);
  auto registry_class = std::make_unique<ClassObject>(53, "Registry");
  registry_class->instance_fields = {Field("size", DataType::LONG, 0)};
  auto referent_inst =
      std::make_unique<ClassInstance>(kReferentInstanceId, 54, 0);
  auto referent_class = std::make_unique<ClassObject>(54, "SomeAllocator");

  parser.class_objects_.emplace(51, std::move(cleaner_class));
  parser.class_objects_.emplace(52, std::move(thunk_class));
  parser.class_objects_.emplace(53, std::move(registry_class));
  parser.class_objects_.emplace(54, std::move(referent_class));
  parser.class_instances_.emplace(kCleanrInstanceId, std::move(cleaner_inst));
  parser.class_instances_.emplace(kThunkInstanceId, std::move(thunk_inst));
  parser.class_instances_.emplace(kRegistryInstanceId,
                                  std::move(registry_inst));
  parser.class_instances_.emplace(kReferentInstanceId,
                                  std::move(referent_inst));

  parser.ResolveClassInstanceReferences();
  parser.ComputeNativeSizeOfObjects();
  auto it = parser.class_instances().find(kReferentInstanceId);
  ASSERT_NE(it, parser.class_instances().end());
  EXPECT_EQ(it->second->base_instance.size, 9876u);
}

}  // namespace tracing
