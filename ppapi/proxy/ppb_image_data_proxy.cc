// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_image_data_proxy.h"

#include <string.h>  // For memcpy

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"

#if !defined(OS_NACL)
#include "skia/ext/platform_canvas.h"
#include "ui/surface/transport_dib.h"
#endif

using ppapi::thunk::PPB_ImageData_API;

namespace ppapi {
namespace proxy {

namespace {

// How ImageData re-use works
// --------------------------
//
// When animating plugins (like video), re-creating image datas for each frame
// and mapping the memory has a high overhead. So we try to re-use these when
// possible.
//
// 1. Plugin makes an asynchronous call that transfers an ImageData to the
//    implementation of some API.
// 2. Plugin frees its ImageData reference. If it doesn't do this we can't
//    re-use it.
// 3. When the last plugin ref of an ImageData is released, we don't actually
//    delete it. Instead we put it on a queue where we hold onto it in the
//    plugin process for a short period of time.
// 4. The API implementation that received the ImageData finishes using it.
//    Without our caching system it would get deleted at this point.
// 5. The proxy in the renderer will send NotifyUnusedImageData back to the
//    plugin process. We check if the given resource is in the queue and mark
//    it as usable.
// 6. When the plugin requests a new image data, we check our queue and if there
//    is a usable ImageData of the right size and format, we'll return it
//    instead of making a new one. It's important that caching is only requested
//    when the size is unlikely to change, so cache hits are high.
//
// Some notes:
//
//  - We only re-use image data when the plugin and host are rapidly exchanging
//    them and the size is likely to remain constant. It should be clear that
//    the plugin is promising that it's done with the image.
//
//  - Theoretically we could re-use them in other cases but the lifetime
//    becomes more difficult to manage. The plugin could have used an ImageData
//    in an arbitrary number of queued up PaintImageData calls which we would
//    have to check.
//
//  - If a flush takes a long time or there are many released image datas
//    accumulating in our queue such that some are deleted, we will have
//    released our reference by the time the renderer notifies us of an unused
//    image data. In this case we just give up.
//
//  - We maintain a per-instance cache. Some pages have many instances of
//    Flash, for example, each of a different size. If they're all animating we
//    want each to get its own image data re-use.
//
//  - We generate new resource IDs when re-use happens to try to avoid weird
//    problems if the plugin messes up its refcounting.

// Keep a cache entry for this many seconds before expiring it. We get an entry
// back from the renderer after an ImageData is swapped out, so it means the
// plugin has to be painting at least two frames for this time interval to
// get caching.
static const int kMaxAgeSeconds = 2;

// ImageDataCacheEntry ---------------------------------------------------------

struct ImageDataCacheEntry {
  ImageDataCacheEntry() : usable(false) {}
  explicit ImageDataCacheEntry(ImageData* i)
      : added_time(base::TimeTicks::Now()), usable(false), image(i) {}

  base::TimeTicks added_time;

  // Set to true when the renderer tells us that it's OK to re-use this image.
  bool usable;

  scoped_refptr<ImageData> image;
};

// ImageDataInstanceCache ------------------------------------------------------

// Per-instance cache of image datas.
class ImageDataInstanceCache {
 public:
  ImageDataInstanceCache() : next_insertion_point_(0) {}

  // These functions have the same spec as the ones in ImageDataCache.
  scoped_refptr<ImageData> Get(PPB_ImageData_Shared::ImageDataType type,
                               int width, int height,
                               PP_ImageDataFormat format);
  void Add(ImageData* image_data);
  void ImageDataUsable(ImageData* image_data);

  // Expires old entries. Returns true if there are still entries in the list,
  // false if this instance cache is now empty.
  bool ExpireEntries();

 private:
  void IncrementInsertionPoint();

  // We'll store this many ImageDatas per instance.
  static const size_t kCacheSize = 2;

  ImageDataCacheEntry images_[kCacheSize];

  // Index into cache where the next item will go.
  size_t next_insertion_point_;
};

scoped_refptr<ImageData> ImageDataInstanceCache::Get(
    PPB_ImageData_Shared::ImageDataType type,
    int width, int height,
    PP_ImageDataFormat format) {
  // Just do a brute-force search since the cache is so small.
  for (size_t i = 0; i < kCacheSize; i++) {
    if (!images_[i].usable)
      continue;
    if (images_[i].image->type() != type)
      continue;
    const PP_ImageDataDesc& desc = images_[i].image->desc();
    if (desc.format == format &&
        desc.size.width == width && desc.size.height == height) {
      scoped_refptr<ImageData> ret(images_[i].image);
      images_[i] = ImageDataCacheEntry();

      // Since we just removed an item, this entry is the best place to insert
      // a subsequent one.
      next_insertion_point_ = i;
      return ret;
    }
  }
  return scoped_refptr<ImageData>();
}

void ImageDataInstanceCache::Add(ImageData* image_data) {
  images_[next_insertion_point_] = ImageDataCacheEntry(image_data);
  IncrementInsertionPoint();
}

void ImageDataInstanceCache::ImageDataUsable(ImageData* image_data) {
  for (size_t i = 0; i < kCacheSize; i++) {
    if (images_[i].image.get() == image_data) {
      images_[i].usable = true;

      // This test is important. The renderer doesn't guarantee how many image
      // datas it has or when it notifies us when one is usable. Its possible
      // to get into situations where it's always telling us the old one is
      // usable, and then the older one immediately gets expired. Therefore,
      // if the next insertion would overwrite this now-usable entry, make the
      // next insertion overwrite some other entry to avoid the replacement.
      if (next_insertion_point_ == i)
        IncrementInsertionPoint();
      return;
    }
  }
}

bool ImageDataInstanceCache::ExpireEntries() {
  base::TimeTicks threshold_time =
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(kMaxAgeSeconds);

  bool has_entry = false;
  for (size_t i = 0; i < kCacheSize; i++) {
    if (images_[i].image.get()) {
      // Entry present.
      if (images_[i].added_time <= threshold_time) {
        // Found an entry to expire.
        images_[i] = ImageDataCacheEntry();
        next_insertion_point_ = i;
      } else {
        // Found an entry that we're keeping.
        has_entry = true;
      }
    }
  }
  return has_entry;
}

void ImageDataInstanceCache::IncrementInsertionPoint() {
  // Go to the next location, wrapping around to get LRU.
  next_insertion_point_++;
  if (next_insertion_point_ >= kCacheSize)
    next_insertion_point_ = 0;
}

// ImageDataCache --------------------------------------------------------------

class ImageDataCache {
 public:
  ImageDataCache() {}
  ~ImageDataCache() {}

  static ImageDataCache* GetInstance();

  // Retrieves an image data from the cache of the specified type, size and
  // format if one exists. If one doesn't exist, this will return a null refptr.
  scoped_refptr<ImageData> Get(PP_Instance instance,
                               PPB_ImageData_Shared::ImageDataType type,
                               int width, int height,
                               PP_ImageDataFormat format);

  // Adds the given image data to the cache. There should be no plugin
  // references to it. This may delete an older item from the cache.
  void Add(ImageData* image_data);

  // Notification from the renderer that the given image data is usable.
  void ImageDataUsable(ImageData* image_data);

  void DidDeleteInstance(PP_Instance instance);

 private:
  friend struct base::LeakySingletonTraits<ImageDataCache>;

  // Timer callback to expire entries for the given instance.
  void OnTimer(PP_Instance instance);

  typedef std::map<PP_Instance, ImageDataInstanceCache> CacheMap;
  CacheMap cache_;

  // This class does timer calls and we don't want to run these outside of the
  // scope of the object. Technically, since this class is a leaked static,
  // this will never happen and this factory is unnecessary. However, it's
  // probably better not to make assumptions about the lifetime of this class.
  base::WeakPtrFactory<ImageDataCache> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImageDataCache);
};

// static
ImageDataCache* ImageDataCache::GetInstance() {
  return base::Singleton<ImageDataCache,
                         base::LeakySingletonTraits<ImageDataCache>>::get();
}

scoped_refptr<ImageData> ImageDataCache::Get(
    PP_Instance instance,
    PPB_ImageData_Shared::ImageDataType type,
    int width, int height,
    PP_ImageDataFormat format) {
  CacheMap::iterator found = cache_.find(instance);
  if (found == cache_.end())
    return scoped_refptr<ImageData>();
  return found->second.Get(type, width, height, format);
}

void ImageDataCache::Add(ImageData* image_data) {
  cache_[image_data->pp_instance()].Add(image_data);

  // Schedule a timer to invalidate this entry.
  PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostDelayedTask(
      FROM_HERE,
      RunWhileLocked(base::Bind(&ImageDataCache::OnTimer,
                                weak_factory_.GetWeakPtr(),
                                image_data->pp_instance())),
      base::TimeDelta::FromSeconds(kMaxAgeSeconds));
}

void ImageDataCache::ImageDataUsable(ImageData* image_data) {
  CacheMap::iterator found = cache_.find(image_data->pp_instance());
  if (found != cache_.end())
    found->second.ImageDataUsable(image_data);
}

void ImageDataCache::DidDeleteInstance(PP_Instance instance) {
  cache_.erase(instance);
}

void ImageDataCache::OnTimer(PP_Instance instance) {
  CacheMap::iterator found = cache_.find(instance);
  if (found == cache_.end())
    return;
  if (!found->second.ExpireEntries()) {
    // There are no more entries for this instance, remove it from the cache.
    cache_.erase(found);
  }
}

}  // namespace

// ImageData -------------------------------------------------------------------

ImageData::ImageData(const HostResource& resource,
                     PPB_ImageData_Shared::ImageDataType type,
                     const PP_ImageDataDesc& desc)
    : Resource(OBJECT_IS_PROXY, resource),
      type_(type),
      desc_(desc),
      is_candidate_for_reuse_(false) {
}

ImageData::~ImageData() {
}

PPB_ImageData_API* ImageData::AsPPB_ImageData_API() {
  return this;
}

void ImageData::LastPluginRefWasDeleted() {
  // The plugin no longer needs this ImageData, add it to our cache if it's
  // been used in a ReplaceContents. These are the ImageDatas that the renderer
  // will send back ImageDataUsable messages for.
  if (is_candidate_for_reuse_)
    ImageDataCache::GetInstance()->Add(this);
}

void ImageData::InstanceWasDeleted() {
  ImageDataCache::GetInstance()->DidDeleteInstance(pp_instance());
}

PP_Bool ImageData::Describe(PP_ImageDataDesc* desc) {
  memcpy(desc, &desc_, sizeof(PP_ImageDataDesc));
  return PP_TRUE;
}

int32_t ImageData::GetSharedMemoryRegion(
    base::UnsafeSharedMemoryRegion** /* region */) {
  // Not supported in the proxy (this method is for actually implementing the
  // proxy in the host).
  return PP_ERROR_NOACCESS;
}

void ImageData::SetIsCandidateForReuse() {
  is_candidate_for_reuse_ = true;
}

void ImageData::RecycleToPlugin(bool zero_contents) {
  is_candidate_for_reuse_ = false;
  if (zero_contents) {
    void* data = Map();
    memset(data, 0, desc_.stride * desc_.size.height);
    Unmap();
  }
}

// PlatformImageData -----------------------------------------------------------

#if !defined(OS_NACL)
PlatformImageData::PlatformImageData(
    const HostResource& resource,
    const PP_ImageDataDesc& desc,
    base::UnsafeSharedMemoryRegion image_region)
    : ImageData(resource, PPB_ImageData_Shared::PLATFORM, desc) {
#if defined(OS_WIN)
  transport_dib_ = TransportDIB::CreateWithHandle(std::move(image_region));
#else
  transport_dib_ = TransportDIB::Map(std::move(image_region));
#endif  // defined(OS_WIN)
}

PlatformImageData::~PlatformImageData() = default;

void* PlatformImageData::Map() {
  if (!mapped_canvas_.get()) {
    if (!transport_dib_.get())
      return nullptr;

    const bool is_opaque = false;
    mapped_canvas_ = transport_dib_->GetPlatformCanvas(
        desc_.size.width, desc_.size.height, is_opaque);
    if (!mapped_canvas_.get())
      return nullptr;
  }
  SkPixmap pixmap;
  skia::GetWritablePixels(mapped_canvas_.get(), &pixmap);
  return pixmap.writable_addr();
}

void PlatformImageData::Unmap() {
  // TODO(brettw) have a way to unmap a TransportDIB. Currently this isn't
  // possible since deleting the TransportDIB also frees all the handles.
  // We need to add a method to TransportDIB to release the handles.
}

SkCanvas* PlatformImageData::GetCanvas() {
  return mapped_canvas_.get();
}
#endif  // !defined(OS_NACL)

// SimpleImageData -------------------------------------------------------------

SimpleImageData::SimpleImageData(const HostResource& resource,
                                 const PP_ImageDataDesc& desc,
                                 base::UnsafeSharedMemoryRegion region)
    : ImageData(resource, PPB_ImageData_Shared::SIMPLE, desc),
      shm_region_(std::move(region)),
      size_(desc.size.width * desc.size.height * 4),
      map_count_(0) {}

SimpleImageData::~SimpleImageData() = default;

void* SimpleImageData::Map() {
  if (map_count_++ == 0)
    shm_mapping_ = shm_region_.MapAt(0, size_);
  return shm_mapping_.IsValid() ? shm_mapping_.memory() : nullptr;
}

void SimpleImageData::Unmap() {
  if (--map_count_ == 0)
    shm_mapping_ = base::WritableSharedMemoryMapping();
}

SkCanvas* SimpleImageData::GetCanvas() {
  return nullptr;  // No canvas available.
}

// PPB_ImageData_Proxy ---------------------------------------------------------

PPB_ImageData_Proxy::PPB_ImageData_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_ImageData_Proxy::~PPB_ImageData_Proxy() {
}

// static
PP_Resource PPB_ImageData_Proxy::CreateProxyResource(
    PP_Instance instance,
    PPB_ImageData_Shared::ImageDataType type,
    PP_ImageDataFormat format,
    const PP_Size& size,
    PP_Bool init_to_zero) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  // Check the cache.
  scoped_refptr<ImageData> cached_image_data =
      ImageDataCache::GetInstance()->Get(instance, type,
                                         size.width, size.height, format);
  if (cached_image_data.get()) {
    // We have one we can re-use rather than allocating a new one.
    cached_image_data->RecycleToPlugin(PP_ToBool(init_to_zero));
    return cached_image_data->GetReference();
  }

  HostResource result;
  PP_ImageDataDesc desc;
  switch (type) {
    case PPB_ImageData_Shared::SIMPLE: {
      ppapi::proxy::SerializedHandle image_handle;
      dispatcher->Send(new PpapiHostMsg_PPBImageData_CreateSimple(
          kApiID, instance, format, size, init_to_zero, &result, &desc,
          &image_handle));
      if (image_handle.is_shmem_region()) {
        base::UnsafeSharedMemoryRegion image_region =
            base::UnsafeSharedMemoryRegion::Deserialize(
                image_handle.TakeSharedMemoryRegion());
        if (!result.is_null()) {
          return (new SimpleImageData(result, desc, std::move(image_region)))
              ->GetReference();
        }
      }
      break;
    }
    case PPB_ImageData_Shared::PLATFORM: {
#if !defined(OS_NACL)
      ppapi::proxy::SerializedHandle image_handle;
      dispatcher->Send(new PpapiHostMsg_PPBImageData_CreatePlatform(
          kApiID, instance, format, size, init_to_zero, &result, &desc,
          &image_handle));
      if (image_handle.is_shmem_region()) {
        base::UnsafeSharedMemoryRegion image_region =
            base::UnsafeSharedMemoryRegion::Deserialize(
                image_handle.TakeSharedMemoryRegion());
        if (!result.is_null()) {
          return (new PlatformImageData(result, desc, std::move(image_region)))
              ->GetReference();
        }
      }
#else
      // PlatformImageData shouldn't be created in untrusted code.
      NOTREACHED();
#endif
      break;
    }
  }

  return 0;
}

bool PPB_ImageData_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_ImageData_Proxy, msg)
#if !defined(OS_NACL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBImageData_CreatePlatform,
                        OnHostMsgCreatePlatform)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBImageData_CreateSimple,
                        OnHostMsgCreateSimple)
#endif
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBImageData_NotifyUnusedImageData,
                        OnPluginMsgNotifyUnusedImageData)

    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

#if !defined(OS_NACL)
// static
PP_Resource PPB_ImageData_Proxy::CreateImageData(
    PP_Instance instance,
    PPB_ImageData_Shared::ImageDataType type,
    PP_ImageDataFormat format,
    const PP_Size& size,
    bool init_to_zero,
    PP_ImageDataDesc* desc,
    base::UnsafeSharedMemoryRegion* image_region) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  thunk::EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;

  PP_Bool pp_init_to_zero = init_to_zero ? PP_TRUE : PP_FALSE;
  PP_Resource pp_resource = 0;
  switch (type) {
    case PPB_ImageData_Shared::SIMPLE: {
      pp_resource = enter.functions()->CreateImageDataSimple(
          instance, format, &size, pp_init_to_zero);
      break;
    }
    case PPB_ImageData_Shared::PLATFORM: {
      pp_resource = enter.functions()->CreateImageData(
          instance, format, &size, pp_init_to_zero);
      break;
    }
  }

  if (!pp_resource)
    return 0;

  ppapi::ScopedPPResource resource(ppapi::ScopedPPResource::PassRef(),
                                   pp_resource);

  thunk::EnterResourceNoLock<PPB_ImageData_API> enter_resource(resource.get(),
                                                               false);
  if (enter_resource.object()->Describe(desc) != PP_TRUE) {
    DVLOG(1) << "CreateImageData failed: could not Describe";
    return 0;
  }

  base::UnsafeSharedMemoryRegion* local_shm;
  if (enter_resource.object()->GetSharedMemoryRegion(&local_shm) != PP_OK) {
    DVLOG(1) << "CreateImageData failed: could not GetSharedMemory";
    return 0;
  }

  *image_region =
      dispatcher->ShareUnsafeSharedMemoryRegionWithRemote(*local_shm);
  return resource.Release();
}

void PPB_ImageData_Proxy::OnHostMsgCreatePlatform(
    PP_Instance instance,
    int32_t format,
    const PP_Size& size,
    PP_Bool init_to_zero,
    HostResource* result,
    PP_ImageDataDesc* desc,
    ppapi::proxy::SerializedHandle* result_image_handle) {
  // Clear |desc| so we don't send uninitialized memory to the plugin.
  // https://crbug.com/391023.
  *desc = PP_ImageDataDesc();
  base::UnsafeSharedMemoryRegion image_region;
  PP_Resource resource =
      CreateImageData(instance, PPB_ImageData_Shared::PLATFORM,
                      static_cast<PP_ImageDataFormat>(format), size,
                      true /* init_to_zero */, desc, &image_region);
  result->SetHostResource(instance, resource);
  if (resource) {
    result_image_handle->set_shmem_region(
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            std::move(image_region)));
  } else {
    result_image_handle->set_null_shmem_region();
  }
}

void PPB_ImageData_Proxy::OnHostMsgCreateSimple(
    PP_Instance instance,
    int32_t format,
    const PP_Size& size,
    PP_Bool init_to_zero,
    HostResource* result,
    PP_ImageDataDesc* desc,
    ppapi::proxy::SerializedHandle* result_image_handle) {
  // Clear |desc| so we don't send uninitialized memory to the plugin.
  // https://crbug.com/391023.
  *desc = PP_ImageDataDesc();
  base::UnsafeSharedMemoryRegion image_region;
  PP_Resource resource =
      CreateImageData(instance, PPB_ImageData_Shared::SIMPLE,
                      static_cast<PP_ImageDataFormat>(format), size,
                      true /* init_to_zero */, desc, &image_region);
  result->SetHostResource(instance, resource);
  if (resource) {
    result_image_handle->set_shmem_region(
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            std::move(image_region)));
  } else {
    result_image_handle->set_null_shmem_region();
  }
}
#endif  // !defined(OS_NACL)

void PPB_ImageData_Proxy::OnPluginMsgNotifyUnusedImageData(
    const HostResource& old_image_data) {
  PluginGlobals* plugin_globals = PluginGlobals::Get();
  if (!plugin_globals) {
    return;  // This may happen if the plugin is maliciously sending this
             // message to the renderer.
  }

  EnterPluginFromHostResource<PPB_ImageData_API> enter(old_image_data);
  if (enter.succeeded()) {
    ImageData* image_data = static_cast<ImageData*>(enter.object());
    ImageDataCache::GetInstance()->ImageDataUsable(image_data);
  }

  // The renderer sent us a reference with the message. If the image data was
  // still cached in our process, the proxy still holds a reference so we can
  // remove the one the renderer just sent is. If the proxy no longer holds a
  // reference, we released everything and we should also release the one the
  // renderer just sent us.
  dispatcher()->Send(new PpapiHostMsg_PPBCore_ReleaseResource(
      API_ID_PPB_CORE, old_image_data));
}

}  // namespace proxy
}  // namespace ppapi
