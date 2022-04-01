// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "gpu/gles2_conform_support/egl/test_support.h"

// This file tests EGL basic interface for command_buffer_gles2, the mode of
// command buffer where the code is compiled as a standalone dynamic library and
// exposed through EGL API.
namespace gpu {

class EGLTest : public testing::Test {
 public:
  void TearDown() override;
};

void EGLTest::TearDown() {
  EXPECT_TRUE(eglReleaseThread());
}

TEST_F(EGLTest, OnlyReleaseThread) {}

TEST_F(EGLTest, GetDisplay) {
  EGLDisplay display1 = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EXPECT_NE(display1, EGL_NO_DISPLAY);

  EGLDisplay display2 = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EXPECT_EQ(display1, display2);

#if defined(USE_OZONE)
  EGLNativeDisplayType invalid_display_type =
      static_cast<EGLNativeDisplayType>(0x1);
#else
  EGLNativeDisplayType invalid_display_type =
      reinterpret_cast<EGLNativeDisplayType>(0x1);
#endif
  EXPECT_NE(invalid_display_type, EGL_DEFAULT_DISPLAY);
  EXPECT_EQ(EGL_NO_DISPLAY, eglGetDisplay(invalid_display_type));
  EXPECT_EQ(EGL_SUCCESS, eglGetError());

  // eglTerminate can be called with uninitialized display.
  EXPECT_TRUE(eglTerminate(display1));
}

TEST_F(EGLTest, GetError) {
  // GetError returns success.
  EXPECT_EQ(EGL_SUCCESS, eglGetError());

  // "calling eglGetError twice without any other intervening EGL calls will
  // always return EGL_SUCCESS on the second call"
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EXPECT_NE(display, EGL_NO_DISPLAY);
  EXPECT_EQ(EGL_SUCCESS, eglGetError());
  EXPECT_EQ(nullptr, eglQueryString(display, EGL_EXTENSIONS));
  EXPECT_EQ(EGL_NOT_INITIALIZED, eglGetError());
  EXPECT_EQ(EGL_SUCCESS, eglGetError());

  EXPECT_TRUE(eglTerminate(display));
  EXPECT_EQ(EGL_SUCCESS, eglGetError());
}

TEST_F(EGLTest, Initialize) {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EXPECT_NE(display, EGL_NO_DISPLAY);

  // Test for no crash even though passing nullptrs for major, minor.
  EXPECT_TRUE(eglInitialize(display, nullptr, nullptr));

  EGLint major = 0;
  EGLint minor = 0;
  EXPECT_TRUE(eglInitialize(display, &major, &minor));
  EXPECT_EQ(major, 1);
  EXPECT_EQ(minor, 4);

  EGLDisplay invalid_display = reinterpret_cast<EGLDisplay>(0x1);
  EXPECT_FALSE(eglInitialize(invalid_display, nullptr, nullptr));
  EXPECT_EQ(EGL_BAD_DISPLAY, eglGetError());
}

TEST_F(EGLTest, Terminate) {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EXPECT_NE(display, EGL_NO_DISPLAY);

  // eglTerminate can be called multiple times without initialization.
  EXPECT_TRUE(eglTerminate(display));
  EXPECT_EQ(EGL_SUCCESS, eglGetError());
  EXPECT_TRUE(eglTerminate(display));
  EXPECT_EQ(EGL_SUCCESS, eglGetError());

  EXPECT_TRUE(eglInitialize(display, nullptr, nullptr));

  // eglTerminate can be called multiple times.
  EXPECT_TRUE(eglTerminate(display));
  EXPECT_EQ(EGL_SUCCESS, eglGetError());
  EXPECT_TRUE(eglTerminate(display));
  EXPECT_EQ(EGL_SUCCESS, eglGetError());

  // After Terminate, an egl call returns not initialized.
  EXPECT_EQ(nullptr, eglQueryString(display, EGL_EXTENSIONS));
  EXPECT_EQ(EGL_NOT_INITIALIZED, eglGetError());

  // Re-initialization of same display.
  EXPECT_TRUE(eglInitialize(display, nullptr, nullptr));
  EXPECT_NE(nullptr, eglQueryString(display, EGL_EXTENSIONS));
  EXPECT_TRUE(eglTerminate(display));

  EGLDisplay invalid_display = reinterpret_cast<EGLDisplay>(0x1);
  EXPECT_FALSE(eglTerminate(invalid_display));
  EXPECT_EQ(EGL_BAD_DISPLAY, eglGetError());
}

TEST_F(EGLTest, QueryString) {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EXPECT_NE(display, EGL_NO_DISPLAY);
  EXPECT_EQ(nullptr, eglQueryString(display, EGL_EXTENSIONS));
  EXPECT_EQ(EGL_NOT_INITIALIZED, eglGetError());
  EXPECT_STREQ("", eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));

  EXPECT_EQ(nullptr, eglQueryString(display, EGL_VERSION));
  EXPECT_EQ(EGL_NOT_INITIALIZED, eglGetError());
  EXPECT_STREQ("1.4", eglQueryString(EGL_NO_DISPLAY, EGL_VERSION));

  EXPECT_EQ(nullptr, eglQueryString(display, EGL_CLIENT_APIS));
  EXPECT_EQ(EGL_NOT_INITIALIZED, eglGetError());
  EXPECT_EQ(nullptr, eglQueryString(EGL_NO_DISPLAY, EGL_CLIENT_APIS));
  EXPECT_EQ(EGL_BAD_DISPLAY, eglGetError());
  EXPECT_EQ(nullptr, eglQueryString(display, EGL_VENDOR));
  EXPECT_EQ(EGL_NOT_INITIALIZED, eglGetError());
  EXPECT_EQ(nullptr, eglQueryString(EGL_NO_DISPLAY, EGL_VENDOR));
  EXPECT_EQ(EGL_BAD_DISPLAY, eglGetError());

  EXPECT_TRUE(eglInitialize(display, nullptr, nullptr));
  EXPECT_EQ(EGL_SUCCESS, eglGetError());

  EXPECT_STREQ("", eglQueryString(display, EGL_EXTENSIONS));
  EXPECT_EQ(EGL_SUCCESS, eglGetError());
  EXPECT_STREQ("1.4", eglQueryString(display, EGL_VERSION));
  EXPECT_EQ(EGL_SUCCESS, eglGetError());
  EXPECT_STREQ("OpenGL_ES", eglQueryString(display, EGL_CLIENT_APIS));
  EXPECT_EQ(EGL_SUCCESS, eglGetError());
  EXPECT_STREQ("Google Inc.", eglQueryString(display, EGL_VENDOR));
  EXPECT_EQ(EGL_SUCCESS, eglGetError());
}

TEST_F(EGLTest, GetConfigsUninitialized) {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EXPECT_NE(display, EGL_NO_DISPLAY);

  EGLint num_config = 0;
  const int kConfigsSize = 5;
  EGLConfig configs[kConfigsSize] = {
      0,
  };

  EXPECT_FALSE(eglGetConfigs(display, configs, kConfigsSize, &num_config));
  EXPECT_EQ(EGL_NOT_INITIALIZED, eglGetError());

  EXPECT_FALSE(eglGetConfigs(display, configs, kConfigsSize, nullptr));
  EXPECT_EQ(EGL_NOT_INITIALIZED, eglGetError());
}

TEST_F(EGLTest, ChooseConfigUninitialized) {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EXPECT_NE(display, EGL_NO_DISPLAY);

  EGLint num_config = 0;
  EGLint attrib_list[] = {EGL_NONE};
  const int kConfigsSize = 5;
  EGLConfig configs[kConfigsSize] = {
      0,
  };

  EXPECT_FALSE(eglChooseConfig(display, attrib_list, configs, kConfigsSize,
                               &num_config));
  EXPECT_EQ(EGL_NOT_INITIALIZED, eglGetError());

  EXPECT_FALSE(
      eglChooseConfig(display, attrib_list, configs, kConfigsSize, nullptr));
  EXPECT_EQ(EGL_NOT_INITIALIZED, eglGetError());
}

class EGLConfigTest : public EGLTest {
 public:
  void SetUp() override;

 protected:
  void CheckConfigsExist(EGLint num_config);

  enum { kConfigsSize = 5 };
  EGLDisplay display_;
  EGLConfig configs_[kConfigsSize];
};

void EGLConfigTest::SetUp() {
  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  ASSERT_NE(display_, EGL_NO_DISPLAY);
  EXPECT_TRUE(eglInitialize(display_, nullptr, nullptr));
  memset(configs_, 0, sizeof(configs_));
}

void EGLConfigTest::CheckConfigsExist(EGLint num_config) {
  EGLint i;
  if (num_config > kConfigsSize)
    num_config = static_cast<EGLint>(kConfigsSize);
  for (i = 0; i < num_config; ++i)
    EXPECT_NE(nullptr, configs_[i]);
  for (; i < kConfigsSize; ++i)
    EXPECT_EQ(nullptr, configs_[i]);
}

TEST_F(EGLConfigTest, GetConfigsBadNumConfigs) {
  EXPECT_FALSE(eglGetConfigs(display_, configs_, kConfigsSize, nullptr));
  EXPECT_EQ(EGL_BAD_PARAMETER, eglGetError());
}

TEST_F(EGLConfigTest, GetConfigsNullConfigs) {
  EGLint num_config = 0;
  EXPECT_TRUE(eglGetConfigs(display_, nullptr, 55, &num_config));
  EXPECT_GT(num_config, 0);
}

TEST_F(EGLConfigTest, GetConfigsZeroConfigsSize) {
  EGLint num_config = 0;
  EXPECT_TRUE(eglGetConfigs(display_, configs_, 0, &num_config));
  EXPECT_GT(num_config, 0);
  EXPECT_EQ(nullptr, configs_[0]);
}

TEST_F(EGLConfigTest, GetConfigs) {
  EGLint num_config = 0;
  EXPECT_TRUE(eglGetConfigs(display_, configs_, kConfigsSize, &num_config));
  EXPECT_GT(num_config, 0);
  CheckConfigsExist(num_config);
}

TEST_F(EGLConfigTest, ChooseConfigBadNumConfigs) {
  EGLint attrib_list[] = {EGL_NONE};
  EXPECT_FALSE(
      eglChooseConfig(display_, attrib_list, configs_, kConfigsSize, nullptr));
  EXPECT_EQ(EGL_BAD_PARAMETER, eglGetError());
}

TEST_F(EGLConfigTest, ChooseConfigNullConfigs) {
  EGLint num_config = 0;
  EGLint attrib_list[] = {EGL_NONE};
  EXPECT_TRUE(eglChooseConfig(display_, attrib_list, nullptr, 55, &num_config));
  EXPECT_GT(num_config, 0);
}

TEST_F(EGLConfigTest, ChooseConfigZeroConfigsSize) {
  EGLint num_config = 0;
  EGLint attrib_list[] = {EGL_NONE};
  EXPECT_TRUE(eglChooseConfig(display_, attrib_list, configs_, 0, &num_config));
  EXPECT_GT(num_config, 0);
  EXPECT_EQ(nullptr, configs_[0]);
}

TEST_F(EGLConfigTest, ChooseConfig) {
  EGLint num_config = 0;
  EGLint attrib_list[] = {EGL_NONE};
  EXPECT_TRUE(eglChooseConfig(display_, attrib_list, configs_, kConfigsSize,
                              &num_config));
  EXPECT_GT(num_config, 0);
  CheckConfigsExist(num_config);
}

TEST_F(EGLConfigTest, ChooseConfigInvalidAttrib) {
  const EGLint kNotModified = 55;
  EGLint num_config = kNotModified;
  EGLint invalid_attrib_list[] = {0xABCD};
  EXPECT_FALSE(eglChooseConfig(display_, invalid_attrib_list, configs_,
                               kConfigsSize, &num_config));
  EXPECT_EQ(EGL_BAD_ATTRIBUTE, eglGetError());
  EXPECT_EQ(kNotModified, num_config);
}

TEST_F(EGLConfigTest, ChooseConfigWindow) {
  EGLint num_config = 0;
  EGLint attrib_list[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE};
  EXPECT_TRUE(eglChooseConfig(display_, attrib_list, configs_, kConfigsSize,
                              &num_config));
  EXPECT_GT(num_config, 0);
  for (int i = 0; i < num_config; ++i) {
    EGLint value = EGL_NONE;
    eglGetConfigAttrib(display_, configs_[i], EGL_SURFACE_TYPE, &value);
    EXPECT_NE(0, value & EGL_WINDOW_BIT);
  }
}

TEST_F(EGLConfigTest, ChooseConfigPBuffer) {
  EGLint num_config = 0;
  EGLint attrib_list[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE};
  EXPECT_TRUE(eglChooseConfig(display_, attrib_list, configs_, kConfigsSize,
                              &num_config));
  EXPECT_GT(num_config, 0);
  for (int i = 0; i < num_config; ++i) {
    EGLint value = EGL_NONE;
    eglGetConfigAttrib(display_, configs_[0], EGL_SURFACE_TYPE, &value);
    EXPECT_NE(0, value & EGL_PBUFFER_BIT);
  }
}

TEST_F(EGLConfigTest, ChooseConfigWindowPBufferNotPossible) {
  EGLint num_config = 0;
  EGLint attrib_list[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT | EGL_WINDOW_BIT,
                          EGL_NONE};
  EXPECT_TRUE(eglChooseConfig(display_, attrib_list, configs_, kConfigsSize,
                              &num_config));
  EXPECT_EQ(0, num_config);
}

TEST_F(EGLConfigTest, ChooseConfigBugExample) {
  static const EGLint kConfigAttribs[] = {
      EGL_RED_SIZE,       8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE,    8,
      EGL_ALPHA_SIZE,     8, EGL_DEPTH_SIZE, 8, EGL_STENCIL_SIZE, 8,
      EGL_SAMPLE_BUFFERS, 1, EGL_SAMPLES,    4, EGL_NONE};
  EGLint num_config = 0;
  EXPECT_TRUE(eglChooseConfig(display_, kConfigAttribs, configs_, kConfigsSize,
                              &num_config));

  // The EGL attribs are not really implemented at the moment.
  EGLint value = EGL_NONE;
  EXPECT_TRUE(eglGetConfigAttrib(display_, configs_[0], EGL_RED_SIZE, &value));
  EXPECT_EQ(0, value);
}

TEST_F(EGLTest, MakeCurrent) {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EXPECT_NE(display, EGL_NO_DISPLAY);
  // "This is the only case where an uninitialized display may be passed to
  //  eglMakeCurrent."
  EXPECT_TRUE(
      eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
  EGLDisplay invalid_display = reinterpret_cast<EGLDisplay>(0x1);
  EXPECT_FALSE(eglMakeCurrent(invalid_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                              EGL_NO_CONTEXT));
  EXPECT_EQ(EGL_BAD_DISPLAY, eglGetError());

  EXPECT_TRUE(eglInitialize(display, nullptr, nullptr));
  EXPECT_TRUE(
      eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
  EXPECT_FALSE(eglMakeCurrent(invalid_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                              EGL_NO_CONTEXT));
}

class EGLSurfaceTest : public EGLTest {
 public:
  void SetUp() override;
  void CreateSurfaceAndContext(EGLSurface* surface, EGLContext* context);

 protected:
  EGLDisplay display_;
};

void EGLSurfaceTest::SetUp() {
  EGLTest::SetUp();
  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EXPECT_TRUE(eglInitialize(display_, nullptr, nullptr));
}

void EGLSurfaceTest::CreateSurfaceAndContext(EGLSurface* surface,
                                             EGLContext* context) {
  static const EGLint config_attribs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                                          EGL_NONE};
  EGLint num_config;
  EGLConfig config;
  EXPECT_TRUE(
      eglChooseConfig(display_, config_attribs, &config, 1, &num_config));
  ASSERT_GT(num_config, 0);
  static const EGLint surface_attribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1,
                                           EGL_NONE};
  *surface = eglCreatePbufferSurface(display_, config, surface_attribs);
  static const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
                                           EGL_NONE};
  *context = eglCreateContext(display_, config, nullptr, context_attribs);
}

class EGLMultipleSurfacesContextsTest : public EGLSurfaceTest {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  EGLSurface surface1_;
  EGLSurface surface2_;
  EGLContext context1_;
  EGLContext context2_;
};

void EGLMultipleSurfacesContextsTest::SetUp() {
  EGLSurfaceTest::SetUp();
  CreateSurfaceAndContext(&surface1_, &context1_);
  CreateSurfaceAndContext(&surface2_, &context2_);
  EXPECT_NE(EGL_NO_SURFACE, surface1_);
  EXPECT_NE(EGL_NO_SURFACE, surface2_);
  EXPECT_NE(surface1_, surface2_);
  EXPECT_NE(EGL_NO_CONTEXT, context1_);
  EXPECT_NE(EGL_NO_CONTEXT, context2_);
  EXPECT_NE(context1_, context2_);
}

void EGLMultipleSurfacesContextsTest::TearDown() {
  EXPECT_TRUE(eglDestroyContext(display_, context1_));
  EXPECT_TRUE(eglDestroySurface(display_, surface1_));
  EXPECT_TRUE(eglDestroyContext(display_, context2_));
  EXPECT_TRUE(eglDestroySurface(display_, surface2_));
  EGLTest::TearDown();
}

TEST_F(EGLMultipleSurfacesContextsTest, NoMakeCurrent) {}

TEST_F(EGLMultipleSurfacesContextsTest, MakeCurrentSurfaces) {
  EXPECT_TRUE(eglMakeCurrent(display_, surface1_, surface1_, context1_));
  EXPECT_TRUE(eglMakeCurrent(display_, surface2_, surface2_, context2_));
  EXPECT_TRUE(eglMakeCurrent(display_, surface1_, surface1_, context2_));
  EXPECT_TRUE(eglMakeCurrent(display_, surface2_, surface2_, context1_));
}

TEST_F(EGLMultipleSurfacesContextsTest, MakeCurrentSameSurface1) {
  EXPECT_TRUE(eglMakeCurrent(display_, surface1_, surface1_, context1_));
  EXPECT_TRUE(eglMakeCurrent(display_, surface1_, surface1_, context2_));
}

TEST_F(EGLMultipleSurfacesContextsTest, MakeCurrentSameSurface2) {
  EXPECT_TRUE(eglMakeCurrent(display_, surface1_, surface1_, context1_));
  EXPECT_TRUE(eglMakeCurrent(display_, surface2_, surface2_, context1_));
  EXPECT_TRUE(eglMakeCurrent(display_, surface2_, surface2_, context2_));
}

TEST_F(EGLMultipleSurfacesContextsTest, MakeCurrentSurfacesAndReleases) {
  EXPECT_TRUE(eglMakeCurrent(display_, surface1_, surface1_, context1_));
  EXPECT_TRUE(
      eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
  EXPECT_TRUE(eglMakeCurrent(display_, surface2_, surface2_, context2_));
  EXPECT_TRUE(
      eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
  EXPECT_TRUE(eglMakeCurrent(display_, surface1_, surface1_, context2_));
  EXPECT_TRUE(
      eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
  EXPECT_TRUE(eglMakeCurrent(display_, surface2_, surface2_, context1_));
  EXPECT_TRUE(
      eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
}

TEST_F(EGLMultipleSurfacesContextsTest, MakeCurrentSurfaceFails) {
  EXPECT_FALSE(eglMakeCurrent(display_, surface1_, surface1_, EGL_NO_CONTEXT));
  EXPECT_EQ(EGL_BAD_CONTEXT, eglGetError());
  EXPECT_FALSE(eglMakeCurrent(display_, surface1_, EGL_NO_SURFACE, context1_));
  EXPECT_EQ(EGL_BAD_SURFACE, eglGetError());
  EXPECT_FALSE(eglMakeCurrent(display_, EGL_NO_SURFACE, surface1_, context1_));
  EXPECT_EQ(EGL_BAD_SURFACE, eglGetError());

  EGLDisplay invalid_display = reinterpret_cast<EGLDisplay>(0x1);
  EGLSurface invalid_surface = reinterpret_cast<EGLSurface>(0x1);
  EGLSurface invalid_context = reinterpret_cast<EGLContext>(0x1);
  EXPECT_FALSE(
      eglMakeCurrent(invalid_display, surface1_, surface1_, context1_));
  EXPECT_EQ(EGL_BAD_DISPLAY, eglGetError());
  EXPECT_FALSE(eglMakeCurrent(display_, surface1_, surface1_, invalid_context));
  EXPECT_EQ(EGL_BAD_CONTEXT, eglGetError());
  EXPECT_FALSE(eglMakeCurrent(display_, surface1_, invalid_surface, context1_));
  EXPECT_EQ(EGL_BAD_SURFACE, eglGetError());
  EXPECT_FALSE(eglMakeCurrent(display_, invalid_surface, surface1_, context1_));
  EXPECT_EQ(EGL_BAD_SURFACE, eglGetError());

  // Command buffer limitation:
  // Different read and draw surfaces fail.
  EXPECT_FALSE(eglMakeCurrent(display_, surface1_, surface2_, context1_));
  EXPECT_EQ(EGL_BAD_MATCH, eglGetError());
}

TEST_F(EGLMultipleSurfacesContextsTest, CallGLOnMultipleContextNoCrash) {
  EXPECT_TRUE(eglMakeCurrent(display_, surface1_, surface1_, context1_));

  typedef void(GL_APIENTRY * glEnableProc)(GLenum);
  glEnableProc glEnable =
      reinterpret_cast<glEnableProc>(eglGetProcAddress("glEnable"));
  EXPECT_NE(nullptr, glEnable);

  glEnable(GL_BLEND);

  EXPECT_TRUE(eglMakeCurrent(display_, surface2_, surface2_, context2_));
  glEnable(GL_BLEND);
}

class EGLThreadTest : public EGLSurfaceTest {
 public:
  EGLThreadTest();
  void SetUp() override;
  void TearDown() override;
  void OtherThreadTearDown(base::WaitableEvent*);
  void OtherThreadMakeCurrent(EGLSurface surface,
                              EGLContext context,
                              EGLBoolean* result,
                              base::WaitableEvent*);
  void OtherThreadGetError(EGLint* result, base::WaitableEvent*);

 protected:
  base::Thread other_thread_;
};

EGLThreadTest::EGLThreadTest()
    : EGLSurfaceTest(), other_thread_("EGLThreadTest thread") {}
void EGLThreadTest::SetUp() {
  EGLSurfaceTest::SetUp();
  other_thread_.Start();
}

void EGLThreadTest::TearDown() {
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  other_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&EGLThreadTest::OtherThreadTearDown,
                                base::Unretained(this), &completion));
  completion.Wait();
  other_thread_.Stop();
  EGLSurfaceTest::TearDown();
}

void EGLThreadTest::OtherThreadTearDown(base::WaitableEvent* completion) {
  EXPECT_TRUE(eglReleaseThread());
  completion->Signal();
}

void EGLThreadTest::OtherThreadMakeCurrent(EGLSurface surface,
                                           EGLContext context,
                                           EGLBoolean* result,
                                           base::WaitableEvent* completion) {
  *result = eglMakeCurrent(display_, surface, surface, context);
  completion->Signal();
}

void EGLThreadTest::OtherThreadGetError(EGLint* result,
                                        base::WaitableEvent* completion) {
  *result = eglGetError();
  completion->Signal();
}

TEST_F(EGLThreadTest, OnlyReleaseThreadInOther) {}

TEST_F(EGLThreadTest, Basic) {
  EGLSurface surface;
  EGLContext context;
  CreateSurfaceAndContext(&surface, &context);
  EXPECT_NE(EGL_NO_SURFACE, surface);
  EXPECT_NE(EGL_NO_CONTEXT, context);

  EXPECT_TRUE(eglMakeCurrent(display_, surface, surface, context));

  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  EGLBoolean result = EGL_FALSE;
  other_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&EGLThreadTest::OtherThreadMakeCurrent,
                                base::Unretained(this), surface, context,
                                &result, &completion));
  completion.Wait();
  EXPECT_FALSE(result);
  EXPECT_EQ(EGL_SUCCESS, eglGetError());

  EGLint error = EGL_NONE;
  other_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&EGLThreadTest::OtherThreadGetError,
                                base::Unretained(this), &error, &completion));
  completion.Wait();
  EXPECT_EQ(EGL_BAD_ACCESS, error);
  EXPECT_EQ(EGL_SUCCESS, eglGetError());

  other_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&EGLThreadTest::OtherThreadGetError,
                                base::Unretained(this), &error, &completion));
  completion.Wait();
  EXPECT_EQ(EGL_SUCCESS, error);

  EXPECT_TRUE(
      eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

  other_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&EGLThreadTest::OtherThreadMakeCurrent,
                                base::Unretained(this), surface, context,
                                &result, &completion));
  completion.Wait();
  EXPECT_TRUE(result);

  EXPECT_FALSE(eglMakeCurrent(display_, surface, surface, context));
  EXPECT_EQ(EGL_BAD_ACCESS, eglGetError());

  EXPECT_TRUE(eglDestroySurface(display_, surface));
  EXPECT_TRUE(eglDestroyContext(display_, context));
}

TEST_F(EGLTest, WindowlessNativeWindows) {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EXPECT_TRUE(eglInitialize(display, nullptr, nullptr));

  static const EGLint config_attribs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                                          EGL_NONE};
  EGLint num_config;
  EGLConfig config;
  EXPECT_TRUE(
      eglChooseConfig(display, config_attribs, &config, 1, &num_config));
  ASSERT_GT(num_config, 0);
  static const EGLint surface_attribs[] = {EGL_NONE};
  CommandBufferGLESSetNextCreateWindowSurfaceCreatesPBuffer(display, 100, 100);
  EGLNativeWindowType win = 0;
  EGLSurface surface =
      eglCreateWindowSurface(display, config, win, surface_attribs);
  EXPECT_NE(EGL_NO_SURFACE, surface);

  // Test that SwapBuffers can be called on windowless window surfaces.

  static const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
                                           EGL_NONE};
  EGLContext context =
      eglCreateContext(display, config, nullptr, context_attribs);
  EXPECT_TRUE(eglMakeCurrent(display, surface, surface, context));
  EXPECT_TRUE(eglSwapBuffers(display, surface));

  EXPECT_TRUE(eglDestroySurface(display, surface));
  EXPECT_TRUE(eglDestroyContext(display, context));
}

}  // namespace gpu
