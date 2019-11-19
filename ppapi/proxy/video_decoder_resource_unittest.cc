// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <stddef.h>
#include <stdint.h>

#include "base/memory/unsafe_shared_memory_region.h"
#include "build/build_config.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_video_decoder.h"
#include "ppapi/proxy/locking_resource_releaser.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/proxy/ppb_graphics_3d_proxy.h"
#include "ppapi/proxy/video_decoder_constants.h"
#include "ppapi/proxy/video_decoder_resource.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/thunk/thunk.h"

using ppapi::proxy::ResourceMessageTestSink;

namespace ppapi {
namespace proxy {

namespace {

const PP_Resource kGraphics3D = 7;
const uint32_t kShmSize = 256;
const size_t kDecodeBufferSize = 16;
const uint32_t kDecodeId = 5;
const uint32_t kTextureId1 = 1;
#if !defined(OS_WIN) || !defined(ARCH_CPU_64_BITS)
const uint32_t kTextureId2 = 2;
#endif
const uint32_t kNumRequestedTextures = 2;

class MockCompletionCallback {
 public:
  MockCompletionCallback() : called_(false) {}

  bool called() { return called_; }
  int32_t result() { return result_; }

  void Reset() { called_ = false; }

  static void Callback(void* user_data, int32_t result) {
    MockCompletionCallback* that =
        reinterpret_cast<MockCompletionCallback*>(user_data);
    that->called_ = true;
    that->result_ = result;
  }

 private:
  bool called_;
  int32_t result_;
};

class VideoDecoderResourceTest : public PluginProxyTest {
 public:
  VideoDecoderResourceTest()
      : decoder_iface_(thunk::GetPPB_VideoDecoder_1_1_Thunk()) {}

  const PPB_VideoDecoder_1_1* decoder_iface() const { return decoder_iface_; }

  void SendReply(const ResourceMessageCallParams& params,
                 int32_t result,
                 const IPC::Message& nested_message) {
    ResourceMessageReplyParams reply_params(params.pp_resource(),
                                            params.sequence());
    reply_params.set_result(result);
    PluginMessageFilter::DispatchResourceReplyForTest(reply_params,
                                                      nested_message);
  }

  PP_Resource CreateDecoder() {
    PP_Resource result = decoder_iface()->Create(pp_instance());
    if (result) {
      ProxyAutoLock lock;
      ppapi::Resource* resource =
          GetGlobals()->GetResourceTracker()->GetResource(result);
      proxy::VideoDecoderResource* decoder =
          static_cast<proxy::VideoDecoderResource*>(resource);
      decoder->SetForTest();
    }

    return result;
  }

  PP_Resource CreateGraphics3d() {
    ProxyAutoLock lock;

    HostResource host_resource;
    host_resource.SetHostResource(pp_instance(), kGraphics3D);
    scoped_refptr<ppapi::proxy::Graphics3D> graphics_3d(
        new ppapi::proxy::Graphics3D(host_resource, gfx::Size(640, 480),
                                     false));

    return graphics_3d->GetReference();
  }

  PP_Resource CreateAndInitializeDecoder() {
    PP_Resource decoder = CreateDecoder();
    LockingResourceReleaser graphics3d(CreateGraphics3d());
    MockCompletionCallback cb;
    int32_t result = decoder_iface()->Initialize(
        decoder,
        graphics3d.get(),
        PP_VIDEOPROFILE_H264MAIN,
        PP_HARDWAREACCELERATION_WITHFALLBACK,
        0,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          &cb));
    if (result != PP_OK_COMPLETIONPENDING)
      return 0;
    ResourceMessageCallParams params;
    IPC::Message msg;
    if (!sink().GetFirstResourceCallMatching(
            PpapiHostMsg_VideoDecoder_Initialize::ID, &params, &msg))
      return 0;
    sink().ClearMessages();
    SendReply(params, PP_OK, PpapiPluginMsg_VideoDecoder_InitializeReply());
    return decoder;
  }

  int32_t CallDecode(PP_Resource pp_decoder,
                     MockCompletionCallback* cb,
                     const PpapiHostMsg_VideoDecoder_GetShm* expected_shm_msg) {
    // Set up a handler in case the resource sends a sync message to create
    // shared memory.
    PpapiPluginMsg_VideoDecoder_GetShmReply shm_msg_reply(kShmSize);
    ResourceSyncCallHandler shm_msg_handler(
        &sink(), PpapiHostMsg_VideoDecoder_GetShm::ID, PP_OK, shm_msg_reply);
    sink().AddFilter(&shm_msg_handler);

    if (expected_shm_msg) {
      auto region = base::UnsafeSharedMemoryRegion::Create(kShmSize);
      auto serialized_handle = std::make_unique<SerializedHandle>(
          base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
              std::move(region)));
      shm_msg_handler.set_serialized_handle(std::move(serialized_handle));
    }

    memset(decode_buffer_, 0x55, kDecodeBufferSize);
    int32_t result =
        decoder_iface()->Decode(pp_decoder,
                                kDecodeId,
                                kDecodeBufferSize,
                                decode_buffer_,
                                PP_MakeOptionalCompletionCallback(
                                    &MockCompletionCallback::Callback, cb));

    if (expected_shm_msg) {
      uint32_t shm_id, shm_size, expected_shm_id, expected_shm_size;
      UnpackMessage<PpapiHostMsg_VideoDecoder_GetShm>(
          *expected_shm_msg, &expected_shm_id, &expected_shm_size);
      if (shm_msg_handler.last_handled_msg().type() == 0 ||
          !UnpackMessage<PpapiHostMsg_VideoDecoder_GetShm>(
              shm_msg_handler.last_handled_msg(), &shm_id, &shm_size) ||
          shm_id != expected_shm_id ||
          shm_size != expected_shm_size) {
        // Signal that the expected shm message wasn't sent by failing.
        result = PP_ERROR_FAILED;
      }
    }

    sink().RemoveFilter(&shm_msg_handler);
    return result;
  }

  int32_t CallGetPicture(PP_Resource pp_decoder,
                         PP_VideoPicture* picture,
                         MockCompletionCallback* cb) {
    int32_t result =
        decoder_iface()->GetPicture(pp_decoder,
                                    picture,
                                    PP_MakeOptionalCompletionCallback(
                                        &MockCompletionCallback::Callback, cb));
    return result;
  }

  void CallRecyclePicture(PP_Resource pp_decoder,
                          const PP_VideoPicture& picture) {
    decoder_iface()->RecyclePicture(pp_decoder, &picture);
  }

  int32_t CallFlush(PP_Resource pp_decoder, MockCompletionCallback* cb) {
    int32_t result =
        decoder_iface()->Flush(pp_decoder,
                               PP_MakeOptionalCompletionCallback(
                                   &MockCompletionCallback::Callback, cb));
    return result;
  }

  int32_t CallReset(PP_Resource pp_decoder, MockCompletionCallback* cb) {
    int32_t result =
        decoder_iface()->Reset(pp_decoder,
                               PP_MakeOptionalCompletionCallback(
                                   &MockCompletionCallback::Callback, cb));
    return result;
  }

  void SendDecodeReply(const ResourceMessageCallParams& params,
                       uint32_t shm_id) {
    SendReply(params, PP_OK, PpapiPluginMsg_VideoDecoder_DecodeReply(shm_id));
  }

  void SendPictureReady(const ResourceMessageCallParams& params,
                        uint32_t decode_count,
                        uint32_t texture_id) {
    PP_Rect visible_rect = PP_MakeRectFromXYWH(0, 0, 640, 480);
    SendReply(params, PP_OK, PpapiPluginMsg_VideoDecoder_PictureReady(
                                 decode_count, texture_id, visible_rect));
  }

  void SendFlushReply(const ResourceMessageCallParams& params) {
    SendReply(params, PP_OK, PpapiPluginMsg_VideoDecoder_FlushReply());
  }

  void SendResetReply(const ResourceMessageCallParams& params) {
    SendReply(params, PP_OK, PpapiPluginMsg_VideoDecoder_ResetReply());
  }

  void SendRequestTextures(const ResourceMessageCallParams& params) {
    SendReply(params, PP_OK,
              PpapiPluginMsg_VideoDecoder_RequestTextures(
                  kNumRequestedTextures, PP_MakeSize(320, 240), GL_TEXTURE_2D));
  }

  void SendNotifyError(const ResourceMessageCallParams& params, int32_t error) {
    SendReply(params, PP_OK, PpapiPluginMsg_VideoDecoder_NotifyError(error));
  }

  bool CheckDecodeMsg(ResourceMessageCallParams* params,
                      uint32_t* shm_id,
                      uint32_t* size,
                      int32_t* decode_id) {
    IPC::Message msg;
    if (!sink().GetFirstResourceCallMatching(
            PpapiHostMsg_VideoDecoder_Decode::ID, params, &msg))
      return false;
    sink().ClearMessages();
    return UnpackMessage<PpapiHostMsg_VideoDecoder_Decode>(
        msg, shm_id, size, decode_id);
  }

  bool CheckRecyclePictureMsg(ResourceMessageCallParams* params,
                              uint32_t* texture_id) {
    IPC::Message msg;
    if (!sink().GetFirstResourceCallMatching(
            PpapiHostMsg_VideoDecoder_RecyclePicture::ID, params, &msg))
      return false;
    sink().ClearMessages();
    return UnpackMessage<PpapiHostMsg_VideoDecoder_RecyclePicture>(msg,
                                                                   texture_id);
  }

  bool CheckFlushMsg(ResourceMessageCallParams* params) {
    return CheckMsg(params, PpapiHostMsg_VideoDecoder_Flush::ID);
  }

  bool CheckResetMsg(ResourceMessageCallParams* params) {
    return CheckMsg(params, PpapiHostMsg_VideoDecoder_Reset::ID);
  }

  void ClearCallbacks(PP_Resource pp_decoder) {
    ResourceMessageCallParams params;
    MockCompletionCallback cb;

    // Reset to abort Decode and GetPicture callbacks.
    CallReset(pp_decoder, &cb);
    // Initialize params so we can reply to the Reset.
    CheckResetMsg(&params);
    // Run the Reset callback.
    SendResetReply(params);
  }

 private:
  bool CheckMsg(ResourceMessageCallParams* params, int id) {
    IPC::Message msg;
    if (!sink().GetFirstResourceCallMatching(id, params, &msg))
      return false;
    sink().ClearMessages();
    return true;
  }

  const PPB_VideoDecoder_1_1* decoder_iface_;

  char decode_buffer_[kDecodeBufferSize];
};

}  // namespace

TEST_F(VideoDecoderResourceTest, Initialize) {
  // Initialize with 0 graphics3d_context should fail.
  {
    LockingResourceReleaser decoder(CreateDecoder());
    MockCompletionCallback cb;
    int32_t result = decoder_iface()->Initialize(
        decoder.get(),
        0 /* invalid 3d graphics */,
        PP_VIDEOPROFILE_H264MAIN,
        PP_HARDWAREACCELERATION_WITHFALLBACK,
        0,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          &cb));
    ASSERT_EQ(PP_ERROR_BADRESOURCE, result);
  }
  // Initialize with bad profile value should fail.
  {
    LockingResourceReleaser decoder(CreateDecoder());
    MockCompletionCallback cb;
    int32_t result = decoder_iface()->Initialize(
        decoder.get(),
        1 /* non-zero resource */,
        static_cast<PP_VideoProfile>(-1),
        PP_HARDWAREACCELERATION_WITHFALLBACK,
        0,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          &cb));
    ASSERT_EQ(PP_ERROR_BADARGUMENT, result);
  }
  // Initialize with valid graphics3d_context and profile should succeed.
  {
    LockingResourceReleaser decoder(CreateDecoder());
    LockingResourceReleaser graphics3d(CreateGraphics3d());
    MockCompletionCallback cb;
    int32_t result = decoder_iface()->Initialize(
        decoder.get(),
        graphics3d.get(),
        PP_VIDEOPROFILE_H264MAIN,
        PP_HARDWAREACCELERATION_WITHFALLBACK,
        0,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          &cb));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
    ASSERT_TRUE(decoder_iface()->IsVideoDecoder(decoder.get()));

    // Another attempt while pending should fail.
    result = decoder_iface()->Initialize(
        decoder.get(),
        graphics3d.get(),
        PP_VIDEOPROFILE_H264MAIN,
        PP_HARDWAREACCELERATION_WITHFALLBACK,
        0,
        PP_MakeOptionalCompletionCallback(&MockCompletionCallback::Callback,
                                          &cb));
    ASSERT_EQ(PP_ERROR_INPROGRESS, result);

    // Check for host message and send a reply to complete initialization.
    ResourceMessageCallParams params;
    IPC::Message msg;
    ASSERT_TRUE(sink().GetFirstResourceCallMatching(
        PpapiHostMsg_VideoDecoder_Initialize::ID, &params, &msg));
    sink().ClearMessages();
    SendReply(params, PP_OK, PpapiPluginMsg_VideoDecoder_InitializeReply());
    ASSERT_TRUE(cb.called());
    ASSERT_EQ(PP_OK, cb.result());
  }
}

TEST_F(VideoDecoderResourceTest, Uninitialized) {
  // Operations on uninitialized decoders should fail.
  LockingResourceReleaser decoder(CreateDecoder());
  MockCompletionCallback uncalled_cb;

  ASSERT_EQ(PP_ERROR_FAILED, CallDecode(decoder.get(), &uncalled_cb, NULL));
  ASSERT_FALSE(uncalled_cb.called());

  ASSERT_EQ(PP_ERROR_FAILED, CallGetPicture(decoder.get(), NULL, &uncalled_cb));
  ASSERT_FALSE(uncalled_cb.called());

  ASSERT_EQ(PP_ERROR_FAILED, CallFlush(decoder.get(), &uncalled_cb));
  ASSERT_FALSE(uncalled_cb.called());

  ASSERT_EQ(PP_ERROR_FAILED, CallReset(decoder.get(), &uncalled_cb));
  ASSERT_FALSE(uncalled_cb.called());
}

// TODO(bbudge) Fix sync message testing on Windows 64 bit builds. The reply
// message for GetShm isn't received, causing Decode to fail.
// http://crbug.com/379260
#if !defined(OS_WIN) || !defined(ARCH_CPU_64_BITS)
TEST_F(VideoDecoderResourceTest, DecodeAndGetPicture) {
  LockingResourceReleaser decoder(CreateAndInitializeDecoder());
  ResourceMessageCallParams params, params2;
  MockCompletionCallback decode_cb, get_picture_cb, uncalled_cb;

  uint32_t shm_id;
  uint32_t decode_size;
  int32_t decode_id;
  // Call Decode until we have the maximum pending, minus one.
  for (uint32_t i = 0; i < kMaximumPendingDecodes - 1; i++) {
    PpapiHostMsg_VideoDecoder_GetShm shm_msg(i, kDecodeBufferSize);
    ASSERT_EQ(PP_OK, CallDecode(decoder.get(), &uncalled_cb, &shm_msg));
    ASSERT_FALSE(uncalled_cb.called());
    CheckDecodeMsg(&params, &shm_id, &decode_size, &decode_id);
    ASSERT_EQ(i, shm_id);
    ASSERT_EQ(kDecodeBufferSize, decode_size);
    // The resource generates uids internally, starting at 1.
    int32_t uid = i + 1;
    ASSERT_EQ(uid, decode_id);
  }
  // Once we've allocated the maximum number of buffers, we must wait.
  PpapiHostMsg_VideoDecoder_GetShm shm_msg(7U, kDecodeBufferSize);
  ASSERT_EQ(PP_OK_COMPLETIONPENDING,
            CallDecode(decoder.get(), &decode_cb, &shm_msg));
  CheckDecodeMsg(&params, &shm_id, &decode_size, &decode_id);
  ASSERT_EQ(7U, shm_id);
  ASSERT_EQ(kDecodeBufferSize, decode_size);

  // Calling Decode when another Decode is pending should fail.
  ASSERT_EQ(PP_ERROR_INPROGRESS, CallDecode(decoder.get(), &uncalled_cb, NULL));
  ASSERT_FALSE(uncalled_cb.called());
  // Free up the first decode buffer.
  SendDecodeReply(params, 0U);
  // The decoder should run the pending callback.
  ASSERT_TRUE(decode_cb.called());
  ASSERT_EQ(PP_OK, decode_cb.result());
  decode_cb.Reset();

  // Now try to get a picture. No picture ready message has been received yet.
  PP_VideoPicture picture;
  ASSERT_EQ(PP_OK_COMPLETIONPENDING,
            CallGetPicture(decoder.get(), &picture, &get_picture_cb));
  ASSERT_FALSE(get_picture_cb.called());
  // Calling GetPicture when another GetPicture is pending should fail.
  ASSERT_EQ(PP_ERROR_INPROGRESS,
            CallGetPicture(decoder.get(), &picture, &uncalled_cb));
  ASSERT_FALSE(uncalled_cb.called());
  // Send 'request textures' message to initialize textures.
  SendRequestTextures(params);
  // Send a picture ready message for Decode call 1. The GetPicture callback
  // should complete.
  SendPictureReady(params, 1U, kTextureId1);
  ASSERT_TRUE(get_picture_cb.called());
  ASSERT_EQ(PP_OK, get_picture_cb.result());
  ASSERT_EQ(kDecodeId, picture.decode_id);
  get_picture_cb.Reset();

  // Send a picture ready message for Decode call 2. Since there is no pending
  // GetPicture call, the picture should be queued.
  SendPictureReady(params, 2U, kTextureId2);
  // The next GetPicture should return synchronously.
  ASSERT_EQ(PP_OK, CallGetPicture(decoder.get(), &picture, &uncalled_cb));
  ASSERT_FALSE(uncalled_cb.called());
  ASSERT_EQ(kDecodeId, picture.decode_id);
}
#endif  // !defined(OS_WIN) || !defined(ARCH_CPU_64_BITS)

// TODO(bbudge) Fix sync message testing on Windows 64 bit builds. The reply
// message for GetShm isn't received, causing Decode to fail.
// http://crbug.com/379260
#if !defined(OS_WIN) || !defined(ARCH_CPU_64_BITS)
TEST_F(VideoDecoderResourceTest, RecyclePicture) {
  LockingResourceReleaser decoder(CreateAndInitializeDecoder());
  ResourceMessageCallParams params;
  MockCompletionCallback decode_cb, get_picture_cb, uncalled_cb;

  // Get to a state where we have a picture to recycle.
  PpapiHostMsg_VideoDecoder_GetShm shm_msg(0U, kDecodeBufferSize);
  ASSERT_EQ(PP_OK, CallDecode(decoder.get(), &decode_cb, &shm_msg));
  uint32_t shm_id;
  uint32_t decode_size;
  int32_t decode_id;
  CheckDecodeMsg(&params, &shm_id, &decode_size, &decode_id);
  SendDecodeReply(params, 0U);
  // Send 'request textures' message to initialize textures.
  SendRequestTextures(params);
  // Call GetPicture and send 'picture ready' message to get a picture to
  // recycle.
  PP_VideoPicture picture;
  ASSERT_EQ(PP_OK_COMPLETIONPENDING,
            CallGetPicture(decoder.get(), &picture, &get_picture_cb));
  SendPictureReady(params, 0U, kTextureId1);
  ASSERT_EQ(kTextureId1, picture.texture_id);

  CallRecyclePicture(decoder.get(), picture);
  uint32_t texture_id;
  ASSERT_TRUE(CheckRecyclePictureMsg(&params, &texture_id));
  ASSERT_EQ(kTextureId1, texture_id);

  ClearCallbacks(decoder.get());
}
#endif  // !defined(OS_WIN) || !defined(ARCH_CPU_64_BITS)

TEST_F(VideoDecoderResourceTest, Flush) {
  LockingResourceReleaser decoder(CreateAndInitializeDecoder());
  ResourceMessageCallParams params, params2;
  MockCompletionCallback flush_cb, get_picture_cb, uncalled_cb;

  ASSERT_EQ(PP_OK_COMPLETIONPENDING, CallFlush(decoder.get(), &flush_cb));
  ASSERT_FALSE(flush_cb.called());
  ASSERT_TRUE(CheckFlushMsg(&params));

  ASSERT_EQ(PP_ERROR_FAILED, CallDecode(decoder.get(), &uncalled_cb, NULL));
  ASSERT_FALSE(uncalled_cb.called());

  // Plugin can call GetPicture while Flush is pending.
  ASSERT_EQ(PP_OK_COMPLETIONPENDING,
            CallGetPicture(decoder.get(), NULL, &get_picture_cb));
  ASSERT_FALSE(get_picture_cb.called());

  ASSERT_EQ(PP_ERROR_INPROGRESS, CallFlush(decoder.get(), &uncalled_cb));
  ASSERT_FALSE(uncalled_cb.called());

  ASSERT_EQ(PP_ERROR_FAILED, CallReset(decoder.get(), &uncalled_cb));
  ASSERT_FALSE(uncalled_cb.called());

  // Plugin can call RecyclePicture while Flush is pending.
  PP_VideoPicture picture;
  picture.texture_id = kTextureId1;
  CallRecyclePicture(decoder.get(), picture);
  uint32_t texture_id;
  ASSERT_TRUE(CheckRecyclePictureMsg(&params2, &texture_id));

  SendFlushReply(params);
  // Any pending GetPicture call is aborted.
  ASSERT_TRUE(get_picture_cb.called());
  ASSERT_EQ(PP_ERROR_ABORTED, get_picture_cb.result());
  ASSERT_TRUE(flush_cb.called());
  ASSERT_EQ(PP_OK, flush_cb.result());
}

// TODO(bbudge) Test Reset when we can run the message loop to get aborted
// callbacks to run.

// TODO(bbudge) Fix sync message testing on Windows 64 bit builds. The reply
// message for GetShm isn't received, causing Decode to fail.
// http://crbug.com/379260
#if !defined(OS_WIN) || !defined(ARCH_CPU_64_BITS)
TEST_F(VideoDecoderResourceTest, NotifyError) {
  LockingResourceReleaser decoder(CreateAndInitializeDecoder());
  ResourceMessageCallParams params;
  MockCompletionCallback decode_cb, get_picture_cb, uncalled_cb;

  // Call Decode and GetPicture to have some pending requests.
  PpapiHostMsg_VideoDecoder_GetShm shm_msg(0U, kDecodeBufferSize);
  ASSERT_EQ(PP_OK, CallDecode(decoder.get(), &decode_cb, &shm_msg));
  ASSERT_FALSE(decode_cb.called());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING,
            CallGetPicture(decoder.get(), NULL, &get_picture_cb));
  ASSERT_FALSE(get_picture_cb.called());

  // Send the decoder resource an unsolicited notify error message. We first
  // need to initialize 'params' so the message is routed to the decoder.
  uint32_t shm_id;
  uint32_t decode_size;
  int32_t decode_id;
  CheckDecodeMsg(&params, &shm_id, &decode_size, &decode_id);
  SendNotifyError(params, PP_ERROR_RESOURCE_FAILED);

  // Any pending message should be run with the reported error.
  ASSERT_TRUE(get_picture_cb.called());
  ASSERT_EQ(PP_ERROR_RESOURCE_FAILED, get_picture_cb.result());

  // All further calls return the reported error.
  ASSERT_EQ(PP_ERROR_RESOURCE_FAILED,
            CallDecode(decoder.get(), &uncalled_cb, NULL));
  ASSERT_FALSE(uncalled_cb.called());
  ASSERT_EQ(PP_ERROR_RESOURCE_FAILED,
            CallGetPicture(decoder.get(), NULL, &uncalled_cb));
  ASSERT_FALSE(uncalled_cb.called());
  ASSERT_EQ(PP_ERROR_RESOURCE_FAILED, CallFlush(decoder.get(), &uncalled_cb));
  ASSERT_FALSE(uncalled_cb.called());
  ASSERT_EQ(PP_ERROR_RESOURCE_FAILED, CallReset(decoder.get(), &uncalled_cb));
  ASSERT_FALSE(uncalled_cb.called());
}
#endif  // !defined(OS_WIN) || !defined(ARCH_CPU_64_BITS)

}  // namespace proxy
}  // namespace ppapi
