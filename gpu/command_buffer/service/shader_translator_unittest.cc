// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_translator.h"

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
namespace gles2 {

class ShaderTranslatorTest : public testing::Test {
 public:
  ShaderTranslatorTest() = default;
  ~ShaderTranslatorTest() override = default;

 protected:
  void SetUp() override {
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
    GTEST_SKIP() << "Angle doesn't support OpenGL on Windows";
#else
    ShBuiltInResources resources;
    sh::InitBuiltInResources(&resources);
    resources.MaxExpressionComplexity = 32;
    resources.MaxCallStackDepth = 32;

    vertex_translator_ = new ShaderTranslator();
    fragment_translator_ = new ShaderTranslator();

    ASSERT_TRUE(vertex_translator_->Init(GL_VERTEX_SHADER, SH_GLES2_SPEC,
                                         &resources, SH_ESSL_OUTPUT, {},
                                         false));
    ASSERT_TRUE(fragment_translator_->Init(GL_FRAGMENT_SHADER, SH_GLES2_SPEC,
                                           &resources, SH_ESSL_OUTPUT, {},
                                           false));
#endif  //  BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
  }
  void TearDown() override {
    vertex_translator_ = nullptr;
    fragment_translator_ = nullptr;
  }

  scoped_refptr<ShaderTranslator> vertex_translator_;
  scoped_refptr<ShaderTranslator> fragment_translator_;
};

class ES3ShaderTranslatorTest : public testing::Test {
 public:
  ES3ShaderTranslatorTest() = default;
  ~ES3ShaderTranslatorTest() override = default;

 protected:
  void SetUp() override {
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
    GTEST_SKIP() << "Angle doesn't support OpenGL on Windows";
#else
    ShBuiltInResources resources;
    sh::InitBuiltInResources(&resources);
    resources.MaxExpressionComplexity = 32;
    resources.MaxCallStackDepth = 32;

    vertex_translator_ = new ShaderTranslator();
    fragment_translator_ = new ShaderTranslator();

    ASSERT_TRUE(vertex_translator_->Init(GL_VERTEX_SHADER, SH_GLES3_SPEC,
                                         &resources, SH_ESSL_OUTPUT, {},
                                         false));
    ASSERT_TRUE(fragment_translator_->Init(GL_FRAGMENT_SHADER, SH_GLES3_SPEC,
                                           &resources, SH_ESSL_OUTPUT, {},
                                           false));
#endif  //  BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
  }
  void TearDown() override {
    vertex_translator_ = nullptr;
    fragment_translator_ = nullptr;
  }

  scoped_refptr<ShaderTranslator> vertex_translator_;
  scoped_refptr<ShaderTranslator> fragment_translator_;
};

TEST_F(ShaderTranslatorTest, ValidVertexShader) {
  const char* shader =
      "void main() {\n"
      "  gl_Position = vec4(1.0);\n"
      "}";

  // A valid shader should be successfully translated.
  std::string info_log, translated_source;
  int shader_version;
  AttributeMap attrib_map;
  UniformMap uniform_map;
  VaryingMap varying_map;
  InterfaceBlockMap interface_block_map;
  OutputVariableList output_variable_list;
  EXPECT_TRUE(vertex_translator_->Translate(
      shader, &info_log, &translated_source, &shader_version, &attrib_map,
      &uniform_map, &varying_map, &interface_block_map, &output_variable_list));

  // Info log must be NULL.
  EXPECT_TRUE(info_log.empty());
  // Translated shader must be valid and non-empty.
  ASSERT_FALSE(translated_source.empty());
  // There should be no attributes, uniforms, and only one built-in
  // varying: gl_Position.
  EXPECT_TRUE(attrib_map.empty());
  EXPECT_TRUE(uniform_map.empty());
  EXPECT_TRUE(interface_block_map.empty());
  EXPECT_EQ(1u, varying_map.size());
  EXPECT_TRUE(output_variable_list.empty());
}

TEST_F(ShaderTranslatorTest, InvalidVertexShader) {
  const char* bad_shader = "foo-bar";
  const char* good_shader =
      "void main() {\n"
      "  gl_Position = vec4(1.0);\n"
      "}";

  // An invalid shader should fail.
  std::string info_log, translated_source;
  int shader_version;
  AttributeMap attrib_map;
  UniformMap uniform_map;
  VaryingMap varying_map;
  InterfaceBlockMap interface_block_map;
  OutputVariableList output_variable_list;
  EXPECT_FALSE(vertex_translator_->Translate(
      bad_shader, &info_log, &translated_source, &shader_version, &attrib_map,
      &uniform_map, &varying_map, &interface_block_map, &output_variable_list));
  // Info log must be valid and non-empty.
  ASSERT_FALSE(info_log.empty());
  // Translated shader must be NULL.
  EXPECT_TRUE(translated_source.empty());
  // There should be no attributes, uniforms, varyings, interface block or
  // name mapping.
  EXPECT_TRUE(attrib_map.empty());
  EXPECT_TRUE(uniform_map.empty());
  EXPECT_TRUE(varying_map.empty());
  EXPECT_TRUE(interface_block_map.empty());
  EXPECT_TRUE(output_variable_list.empty());

  // Try a good shader after bad.
  info_log.clear();
  EXPECT_TRUE(vertex_translator_->Translate(
      good_shader, &info_log, &translated_source, &shader_version, &attrib_map,
      &uniform_map, &varying_map, &interface_block_map, &output_variable_list));
  EXPECT_TRUE(info_log.empty());
  EXPECT_FALSE(translated_source.empty());
  EXPECT_TRUE(interface_block_map.empty());
}

TEST_F(ShaderTranslatorTest, ValidFragmentShader) {
  const char* shader =
      "void main() {\n"
      "  gl_FragColor = vec4(1.0);\n"
      "}";

  // A valid shader should be successfully translated.
  std::string info_log, translated_source;
  int shader_version;
  AttributeMap attrib_map;
  UniformMap uniform_map;
  VaryingMap varying_map;
  InterfaceBlockMap interface_block_map;
  OutputVariableList output_variable_list;
  EXPECT_TRUE(fragment_translator_->Translate(
      shader, &info_log, &translated_source, &shader_version, &attrib_map,
      &uniform_map, &varying_map, &interface_block_map, &output_variable_list));
  // Info log must be NULL.
  EXPECT_TRUE(info_log.empty());
  // Translated shader must be valid and non-empty.
  ASSERT_FALSE(translated_source.empty());
  // There should be no attributes, uniforms, varyings, interface block or
  // name mapping.
  EXPECT_TRUE(attrib_map.empty());
  EXPECT_TRUE(uniform_map.empty());
  EXPECT_TRUE(varying_map.empty());
  EXPECT_TRUE(interface_block_map.empty());
  // gl_FragColor.
  EXPECT_EQ(1u, output_variable_list.size());
}

TEST_F(ShaderTranslatorTest, InvalidFragmentShader) {
  const char* shader = "foo-bar";

  std::string info_log, translated_source;
  int shader_version;
  AttributeMap attrib_map;
  UniformMap uniform_map;
  VaryingMap varying_map;
  InterfaceBlockMap interface_block_map;
  OutputVariableList output_variable_list;
  // An invalid shader should fail.
  EXPECT_FALSE(fragment_translator_->Translate(
      shader, &info_log, &translated_source, &shader_version, &attrib_map,
      &uniform_map, &varying_map, &interface_block_map, &output_variable_list));
  // Info log must be valid and non-empty.
  EXPECT_FALSE(info_log.empty());
  // Translated shader must be NULL.
  EXPECT_TRUE(translated_source.empty());
  // There should be no attributes, uniforms, varyings, interface block or
  // name mapping.
  EXPECT_TRUE(attrib_map.empty());
  EXPECT_TRUE(uniform_map.empty());
  EXPECT_TRUE(varying_map.empty());
  EXPECT_TRUE(output_variable_list.empty());
}

TEST_F(ShaderTranslatorTest, GetAttributes) {
  const char* shader =
      "attribute vec4 vPosition;\n"
      "void main() {\n"
      "  gl_Position = vPosition;\n"
      "}";

  std::string info_log, translated_source;
  int shader_version;
  AttributeMap attrib_map;
  UniformMap uniform_map;
  VaryingMap varying_map;
  InterfaceBlockMap interface_block_map;
  OutputVariableList output_variable_list;
  EXPECT_TRUE(vertex_translator_->Translate(
      shader, &info_log, &translated_source, &shader_version, &attrib_map,
      &uniform_map, &varying_map, &interface_block_map, &output_variable_list));
  // Info log must be NULL.
  EXPECT_TRUE(info_log.empty());
  // Translated shader must be valid and non-empty.
  EXPECT_FALSE(translated_source.empty());
  // There should be no uniforms.
  EXPECT_TRUE(uniform_map.empty());
  // There should be no interface blocks.
  EXPECT_TRUE(interface_block_map.empty());
  // There should be one attribute with following characteristics:
  // name:vPosition type:GL_FLOAT_VEC4 size:0.
  EXPECT_EQ(1u, attrib_map.size());
  // The shader translator adds a "_u" prefix to user-defined names.
  AttributeMap::const_iterator iter = attrib_map.find("_uvPosition");
  EXPECT_TRUE(iter != attrib_map.end());
  EXPECT_EQ(static_cast<GLenum>(GL_FLOAT_VEC4), iter->second.type);
  EXPECT_EQ(0u, iter->second.getOutermostArraySize());
  EXPECT_EQ("vPosition", iter->second.name);
}

TEST_F(ShaderTranslatorTest, GetUniforms) {
  const char* shader =
      "precision mediump float;\n"
      "struct Foo {\n"
      "  vec4 color[1];\n"
      "};\n"
      "struct Bar {\n"
      "  Foo foo;\n"
      "};\n"
      "uniform Bar bar[2];\n"
      "void main() {\n"
      "  gl_FragColor = bar[0].foo.color[0] + bar[1].foo.color[0];\n"
      "}";

  std::string info_log, translated_source;
  int shader_version;
  AttributeMap attrib_map;
  UniformMap uniform_map;
  VaryingMap varying_map;
  InterfaceBlockMap interface_block_map;
  OutputVariableList output_variable_list;
  EXPECT_TRUE(fragment_translator_->Translate(
      shader, &info_log, &translated_source, &shader_version, &attrib_map,
      &uniform_map, &varying_map, &interface_block_map, &output_variable_list));
  // Info log must be NULL.
  EXPECT_TRUE(info_log.empty());
  // Translated shader must be valid and non-empty.
  EXPECT_FALSE(translated_source.empty());
  // There should be no attributes.
  EXPECT_TRUE(attrib_map.empty());
  // There should be no interface blocks.
  EXPECT_TRUE(interface_block_map.empty());
  // There should be two uniforms with following characteristics:
  // 1. name:bar[0].foo.color[0] type:GL_FLOAT_VEC4 size:1
  // 2. name:bar[1].foo.color[0] type:GL_FLOAT_VEC4 size:1
  // However, there will be only one entry "bar" in the map.
  EXPECT_EQ(1u, uniform_map.size());
  // The shader translator adds a "_u" prefix to user-defined names.
  UniformMap::const_iterator iter = uniform_map.find("_ubar");
  EXPECT_TRUE(iter != uniform_map.end());
  // First uniform.
  const sh::ShaderVariable* info;
  std::string original_name;
  EXPECT_TRUE(iter->second.findInfoByMappedName("_ubar[0]._ufoo._ucolor[0]",
                                                &info, &original_name));
  EXPECT_EQ(static_cast<GLenum>(GL_FLOAT_VEC4), info->type);
  EXPECT_EQ(1u, info->getOutermostArraySize());
  EXPECT_STREQ("color", info->name.c_str());
  EXPECT_STREQ("bar[0].foo.color[0]", original_name.c_str());
  // Second uniform.
  EXPECT_TRUE(iter->second.findInfoByMappedName("_ubar[1]._ufoo._ucolor[0]",
                                                &info, &original_name));
  EXPECT_EQ(static_cast<GLenum>(GL_FLOAT_VEC4), info->type);
  EXPECT_EQ(1u, info->getOutermostArraySize());
  EXPECT_STREQ("color", info->name.c_str());
  EXPECT_STREQ("bar[1].foo.color[0]", original_name.c_str());
  EXPECT_EQ(1u, output_variable_list.size());
  ASSERT_TRUE(output_variable_list.size() > 0);
  EXPECT_EQ(output_variable_list[0].mappedName, "gl_FragColor");
}


TEST_F(ES3ShaderTranslatorTest, InvalidInterfaceBlocks) {
  const char* shader =
      "#version 300 es\n"
      "precision mediump float;\n"
      "layout(location=0) out vec4 oColor;\n"
      "uniform Color {\n"
      "  float red;\n"
      "  float green;\n"
      "  float blue;\n"
      "};\n"
      "uniform Color2 {\n"
      "  float R;\n"
      "  float green;\n"
      "  float B;\n"
      "};\n"
      "void main() {\n"
      "  oColor = vec4(red * R, green * green, blue * B, 1.0);\n"
      "}";

  std::string info_log, translated_source;
  int shader_version;
  AttributeMap attrib_map;
  UniformMap uniform_map;
  VaryingMap varying_map;
  InterfaceBlockMap interface_block_map;
  OutputVariableList output_variable_list;
  EXPECT_FALSE(fragment_translator_->Translate(
      shader, &info_log, &translated_source, &shader_version, &attrib_map,
      &uniform_map, &varying_map, &interface_block_map, &output_variable_list));
  // Info log must be valid and non-empty.
  ASSERT_FALSE(info_log.empty());
  // Translated shader must be NULL.
  EXPECT_TRUE(translated_source.empty());
  // There should be no attributes, uniforms, varyings, interface block or
  // name mapping.
  EXPECT_TRUE(attrib_map.empty());
  EXPECT_TRUE(uniform_map.empty());
  EXPECT_TRUE(varying_map.empty());
  EXPECT_TRUE(interface_block_map.empty());
}

TEST_F(ES3ShaderTranslatorTest, GetInterfaceBlocks) {
  const char* shader =
      "#version 300 es\n"
      "precision mediump float;\n"
      "layout(location=0) out vec4 oColor;\n"
      "uniform Color {\n"
      "  float red;\n"
      "  float green;\n"
      "  float blue;\n"
      "};\n"
      "void main() {\n"
      "  oColor = vec4(red, green, blue, 1.0);\n"
      "}";

  std::string info_log, translated_source;
  int shader_version;
  AttributeMap attrib_map;
  UniformMap uniform_map;
  VaryingMap varying_map;
  InterfaceBlockMap interface_block_map;
  OutputVariableList output_variable_list;
  EXPECT_TRUE(fragment_translator_->Translate(
      shader, &info_log, &translated_source, &shader_version, &attrib_map,
      &uniform_map, &varying_map, &interface_block_map, &output_variable_list));
  // Info log must be NULL.
  EXPECT_TRUE(info_log.empty());
  // Translated shader must be valid and non-empty.
  EXPECT_FALSE(translated_source.empty());
  // There should be no attributes.
  EXPECT_TRUE(attrib_map.empty());
  // There should be one block in interface_block_map
  EXPECT_EQ(1u, interface_block_map.size());
  InterfaceBlockMap::const_iterator iter;
  for (iter = interface_block_map.begin();
       iter != interface_block_map.end(); ++iter) {
    if (iter->second.name == "Color")
      break;
  }
  EXPECT_TRUE(iter != interface_block_map.end());
}

TEST_F(ShaderTranslatorTest, OptionsString) {
  scoped_refptr<ShaderTranslator> translator_1 = new ShaderTranslator();
  scoped_refptr<ShaderTranslator> translator_2 = new ShaderTranslator();
  scoped_refptr<ShaderTranslator> translator_3 = new ShaderTranslator();

  ShBuiltInResources resources;
  sh::InitBuiltInResources(&resources);

  ShCompileOptions with_init_output_variables{};
  with_init_output_variables.initOutputVariables = true;

  ASSERT_TRUE(translator_1->Init(GL_VERTEX_SHADER, SH_GLES2_SPEC, &resources,
                                 SH_GLSL_150_CORE_OUTPUT, {}, false));
  ASSERT_TRUE(translator_2->Init(GL_FRAGMENT_SHADER, SH_GLES2_SPEC, &resources,
                                 SH_GLSL_150_CORE_OUTPUT,
                                 with_init_output_variables, false));
  resources.EXT_draw_buffers = 1;
  ASSERT_TRUE(translator_3->Init(GL_VERTEX_SHADER, SH_GLES2_SPEC, &resources,
                                 SH_GLSL_150_CORE_OUTPUT, {}, false));

  std::string options_1(
      translator_1->GetStringForOptionsThatWouldAffectCompilation()->data);
  std::string options_2(
      translator_1->GetStringForOptionsThatWouldAffectCompilation()->data);
  std::string options_3(
      translator_2->GetStringForOptionsThatWouldAffectCompilation()->data);
  std::string options_4(
      translator_3->GetStringForOptionsThatWouldAffectCompilation()->data);

  EXPECT_EQ(options_1, options_2);
  EXPECT_NE(options_1, options_3);
  EXPECT_NE(options_1, options_4);
  EXPECT_NE(options_3, options_4);
}

class ShaderTranslatorOutputVersionTest
    : public testing::TestWithParam<testing::tuple<const char*, const char*>> {
 public:
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
  void SetUp() override {
    GTEST_SKIP() << "Angle doesn't support OpenGL on Windows";
  }
#endif
};

// crbug.com/540543
// https://bugs.chromium.org/p/angleproject/issues/detail?id=1276
// https://bugs.chromium.org/p/angleproject/issues/detail?id=1277
TEST_F(ShaderTranslatorOutputVersionTest, DISABLED_CompatibilityOutput) {
  ShBuiltInResources resources;
  sh::InitBuiltInResources(&resources);

  ShCompileOptions compile_options{};
  compile_options.objectCode = true;

  ShShaderOutput shader_output_language = SH_GLSL_COMPATIBILITY_OUTPUT;
  scoped_refptr<ShaderTranslator> vertex_translator = new ShaderTranslator();
  ASSERT_TRUE(vertex_translator->Init(GL_VERTEX_SHADER, SH_GLES2_SPEC,
                                      &resources, shader_output_language,
                                      compile_options,
                                      false));
  scoped_refptr<ShaderTranslator> fragment_translator = new ShaderTranslator();
  ASSERT_TRUE(fragment_translator->Init(GL_FRAGMENT_SHADER, SH_GLES2_SPEC,
                                        &resources, shader_output_language,
                                        compile_options,
                                        false));

  std::string translated_source;
  int shader_version;
  {
    const char* kShader =
        "attribute vec4 vPosition;\n"
        "void main() {\n"
        "}";

    EXPECT_TRUE(vertex_translator->Translate(
        kShader, nullptr, &translated_source, &shader_version, nullptr, nullptr,
        nullptr, nullptr, nullptr));
    EXPECT_TRUE(translated_source.find("#version") == std::string::npos);
    if (!base::Contains(translated_source, "gl_Position =")) {
      ADD_FAILURE() << "Did not find gl_Position initialization.";
      LOG(ERROR) << "Generated output:\n" << translated_source;
    }
  }
  {
    const char* kShader =
        "#pragma STDGL invariant(all)\n"
        "precision mediump float;\n"
        "varying vec4 v_varying;\n"
        "void main() {\n"
        "    gl_FragColor = v_varying;\n"
        "}\n";

    EXPECT_TRUE(fragment_translator->Translate(
        kShader, nullptr, &translated_source, &shader_version, nullptr, nullptr,
        nullptr, nullptr, nullptr));
    EXPECT_TRUE(base::Contains(translated_source, "#version 120"));
    if (base::Contains(translated_source, "#pragma STDGL invariant(all)")) {
      ADD_FAILURE() << "Found forbidden pragma.";
      LOG(ERROR) << "Generated output:\n" << translated_source;
    }
  }
}

TEST_P(ShaderTranslatorOutputVersionTest, HasCorrectOutputGLSLVersion) {
  // Test that translating to a shader targeting certain OpenGL context version
  // (version string in test param tuple index 0) produces a GLSL shader that
  // contains correct version string for that context (version directive
  // in test param tuple index 1).

  const char* kShader =
      "attribute vec4 vPosition;\n"
      "void main() {\n"
      "  gl_Position = vPosition;\n"
      "}";

  scoped_refptr<ShaderTranslator> translator = new ShaderTranslator();
  ShBuiltInResources resources;
  sh::InitBuiltInResources(&resources);

  ShCompileOptions compile_options{};
  compile_options.objectCode = true;

  ASSERT_TRUE(translator->Init(GL_VERTEX_SHADER, SH_GLES2_SPEC, &resources,
                               SH_ESSL_OUTPUT, compile_options, false));

  std::string translated_source;
  int shader_version;
  EXPECT_TRUE(translator->Translate(kShader, nullptr, &translated_source,
                                    &shader_version, nullptr, nullptr, nullptr,
                                    nullptr, nullptr));

  std::string expected_version_directive = testing::get<1>(GetParam());
  if (expected_version_directive.empty()) {
    EXPECT_TRUE(!base::Contains(translated_source, "#version"))
        << "Translation was:\n"
        << translated_source;
  } else {
    EXPECT_TRUE(base::Contains(translated_source, expected_version_directive))
        << "Translation was:\n"
        << translated_source;
  }
}

// For some compilers, using make_tuple("a", "bb") would end up
// instantiating make_tuple<char[1], char[2]>. This does not work.
namespace {
testing::tuple<const char*, const char*> make_gl_glsl_tuple(
    const char* gl_version,
    const char* glsl_version_directive) {
  return testing::make_tuple(gl_version, glsl_version_directive);
}
}

// Test data for the above test. Check that for the OpenGL ES output
// contexts, the shader is such that GLSL 1.0 is used. The translator
// selects GLSL 1.0 by not output any version at the moment, though we
// do not know if that would be correct for the future OpenGL ES specs.
INSTANTIATE_TEST_SUITE_P(
    OpenGLESContexts,
    ShaderTranslatorOutputVersionTest,
    testing::Values(make_gl_glsl_tuple("opengl es 2.0", ""),
                    make_gl_glsl_tuple("opengl es 3.0", ""),
                    make_gl_glsl_tuple("opengl es 3.1", ""),
                    make_gl_glsl_tuple("opengl es 3.2", "")));

}  // namespace gles2
}  // namespace gpu
