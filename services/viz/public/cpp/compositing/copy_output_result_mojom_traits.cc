// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/copy_output_result_mojom_traits.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/optional_as_pointer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {

// This class retains the ReleaseCallback of the CopyOutputResult that is being
// sent over mojo. A PendingRemote<TextureReleaser> that talks to this impl
// object will be sent over mojo instead of the release_callback_ (which is not
// serializable). Once the client calls Release, the release_callback_ will be
// called. An object of this class will remain alive until the MessagePipe
// attached to it goes away (i.e. SelfOwnedReceiver is used).
class TextureReleaserImpl : public viz::mojom::TextureReleaser {
 public:
  explicit TextureReleaserImpl(viz::ReleaseCallback release_callback)
      : release_callback_(std::move(release_callback)) {}

  // mojom::TextureReleaser implementation:
  void Release(const gpu::SyncToken& sync_token, bool is_lost) override {
    std::move(release_callback_).Run(sync_token, is_lost);
  }

 private:
  viz::ReleaseCallback release_callback_;
};

void Release(mojo::PendingRemote<viz::mojom::TextureReleaser> pending_remote,
             const gpu::SyncToken& sync_token,
             bool is_lost) {
  mojo::Remote<viz::mojom::TextureReleaser> remote(std::move(pending_remote));
  remote->Release(sync_token, is_lost);
}

}  // namespace

namespace mojo {

// static
viz::mojom::CopyOutputResultFormat
EnumTraits<viz::mojom::CopyOutputResultFormat, viz::CopyOutputResult::Format>::
    ToMojom(viz::CopyOutputResult::Format format) {
  switch (format) {
    case viz::CopyOutputResult::Format::RGBA:
      return viz::mojom::CopyOutputResultFormat::RGBA;
    case viz::CopyOutputResult::Format::I420_PLANES:
    case viz::CopyOutputResult::Format::NV12:
      break;  // Not intended for transport across service boundaries.
  }
  NOTREACHED_IN_MIGRATION();
  return viz::mojom::CopyOutputResultFormat::RGBA;
}

// static
bool EnumTraits<viz::mojom::CopyOutputResultFormat,
                viz::CopyOutputResult::Format>::
    FromMojom(viz::mojom::CopyOutputResultFormat input,
              viz::CopyOutputResult::Format* out) {
  switch (input) {
    case viz::mojom::CopyOutputResultFormat::RGBA:
      *out = viz::CopyOutputResult::Format::RGBA;
      return true;
  }
  return false;
}

// static
viz::mojom::CopyOutputResultDestination
EnumTraits<viz::mojom::CopyOutputResultDestination,
           viz::CopyOutputResult::Destination>::
    ToMojom(viz::CopyOutputResult::Destination destination) {
  switch (destination) {
    case viz::CopyOutputResult::Destination::kSystemMemory:
      return viz::mojom::CopyOutputResultDestination::kSystemMemory;
    case viz::CopyOutputResult::Destination::kNativeTextures:
      return viz::mojom::CopyOutputResultDestination::kNativeTextures;
  }
}

// static
bool EnumTraits<viz::mojom::CopyOutputResultDestination,
                viz::CopyOutputResult::Destination>::
    FromMojom(viz::mojom::CopyOutputResultDestination input,
              viz::CopyOutputResult::Destination* out) {
  switch (input) {
    case viz::mojom::CopyOutputResultDestination::kSystemMemory:
      *out = viz::CopyOutputResult::Destination::kSystemMemory;
      return true;
    case viz::mojom::CopyOutputResultDestination::kNativeTextures:
      *out = viz::CopyOutputResult::Destination::kNativeTextures;
      return true;
  }
  return false;
}

// static
viz::CopyOutputResult::Format
StructTraits<viz::mojom::CopyOutputResultDataView,
             std::unique_ptr<viz::CopyOutputResult>>::
    format(const std::unique_ptr<viz::CopyOutputResult>& result) {
  return result->format();
}

// static
viz::CopyOutputResult::Destination
StructTraits<viz::mojom::CopyOutputResultDataView,
             std::unique_ptr<viz::CopyOutputResult>>::
    destination(const std::unique_ptr<viz::CopyOutputResult>& result) {
  return result->destination();
}

// static
const gfx::Rect& StructTraits<viz::mojom::CopyOutputResultDataView,
                              std::unique_ptr<viz::CopyOutputResult>>::
    rect(const std::unique_ptr<viz::CopyOutputResult>& result) {
  return result->rect();
}

// static
std::optional<viz::CopyOutputResult::ScopedSkBitmap>
StructTraits<viz::mojom::CopyOutputResultDataView,
             std::unique_ptr<viz::CopyOutputResult>>::
    bitmap(const std::unique_ptr<viz::CopyOutputResult>& result) {
  if (result->destination() !=
      viz::CopyOutputResult::Destination::kSystemMemory)
    return std::nullopt;
  auto scoped_bitmap = result->ScopedAccessSkBitmap();
  if (!scoped_bitmap.bitmap().readyToDraw()) {
    // During shutdown or switching to background on Android, Chrome will
    // release GPU context, it will release mapped GPU memory which is used
    // in SkBitmap, in that case, a null bitmap will be sent.
    return std::nullopt;
  }
  return scoped_bitmap;
}

// static
mojo::OptionalAsPointer<const gpu::Mailbox>
StructTraits<viz::mojom::CopyOutputResultDataView,
             std::unique_ptr<viz::CopyOutputResult>>::
    mailbox(const std::unique_ptr<viz::CopyOutputResult>& result) {
  if (result->destination() !=
          viz::CopyOutputResult::Destination::kNativeTextures ||
      result->IsEmpty()) {
    return nullptr;
  }

  // Only RGBA can travel across process boundaries.
  DCHECK_EQ(result->format(), viz::CopyOutputResult::Format::RGBA);
  return mojo::OptionalAsPointer(&result->GetTextureResult()->mailbox);
}

// static
mojo::OptionalAsPointer<const gfx::ColorSpace>
StructTraits<viz::mojom::CopyOutputResultDataView,
             std::unique_ptr<viz::CopyOutputResult>>::
    color_space(const std::unique_ptr<viz::CopyOutputResult>& result) {
  if (result->destination() !=
          viz::CopyOutputResult::Destination::kNativeTextures ||
      result->IsEmpty()) {
    return nullptr;
  }
  return mojo::OptionalAsPointer(&result->GetTextureResult()->color_space);
}

// static
mojo::PendingRemote<viz::mojom::TextureReleaser>
StructTraits<viz::mojom::CopyOutputResultDataView,
             std::unique_ptr<viz::CopyOutputResult>>::
    releaser(const std::unique_ptr<viz::CopyOutputResult>& result) {
  if (result->destination() !=
      viz::CopyOutputResult::Destination::kNativeTextures)
    return mojo::NullRemote();

  // Only RGBA can travel across process boundaries, in which case there will be
  // at most one release callback set in the |result|:
  DCHECK_EQ(result->format(), viz::CopyOutputResult::Format::RGBA);
  viz::CopyOutputResult::ReleaseCallbacks release_callbacks =
      result->TakeTextureOwnership();
  // Callbacks can be empty (in case the result is empty), or have exactly 1
  // element (because a result with RGBA format can carry 1 texture).
  DCHECK(release_callbacks.empty() || release_callbacks.size() == 1);

  mojo::PendingRemote<viz::mojom::TextureReleaser> releaser;
  MakeSelfOwnedReceiver(
      std::make_unique<TextureReleaserImpl>(
          release_callbacks.empty() ? viz::ReleaseCallback{}
                                    : std::move(release_callbacks[0])),
      releaser.InitWithNewPipeAndPassReceiver());
  return releaser;
}

// static
bool StructTraits<viz::mojom::CopyOutputResultDataView,
                  std::unique_ptr<viz::CopyOutputResult>>::
    Read(viz::mojom::CopyOutputResultDataView data,
         std::unique_ptr<viz::CopyOutputResult>* out_p) {
  // First read into local variables, and then instantiate an appropriate
  // implementation of viz::CopyOutputResult.
  viz::CopyOutputResult::Format format;
  viz::CopyOutputResult::Destination destination;
  gfx::Rect rect;

  if (!data.ReadFormat(&format) || !data.ReadDestination(&destination) ||
      !data.ReadRect(&rect)) {
    return false;
  }

  if (rect.IsEmpty()) {
    // An empty rect implies an empty result.
    *out_p = std::make_unique<viz::CopyOutputResult>(format, destination,
                                                     gfx::Rect(), false);
    return true;
  }

  switch (format) {
    case viz::CopyOutputResult::Format::RGBA:
      switch (destination) {
        case viz::CopyOutputResult::Destination::kSystemMemory: {
          std::optional<SkBitmap> bitmap_opt;
          if (!data.ReadBitmap(&bitmap_opt))
            return false;
          if (!bitmap_opt) {
            // During shutdown or switching to background on Android, Chrome
            // will release GPU context, it will release mapped GPU memory which
            // is used in SkBitmap, in that case, the sender will send a null
            // bitmap. So we should consider the copy output result is empty.
            *out_p = std::make_unique<viz::CopyOutputResult>(
                format, destination, gfx::Rect(), false);
            return true;
          }
          if (!bitmap_opt->readyToDraw())
            return false;

          *out_p = std::make_unique<viz::CopyOutputSkBitmapResult>(
              rect, std::move(*bitmap_opt));
          return true;
        }

        case viz::CopyOutputResult::Destination::kNativeTextures: {
          std::optional<gpu::Mailbox> mailbox;
          if (!data.ReadMailbox(&mailbox) || !mailbox)
            return false;
          std::optional<gfx::ColorSpace> color_space;
          if (!data.ReadColorSpace(&color_space) || !color_space)
            return false;

          if (mailbox->IsZero()) {
            // Returns an empty result.
            *out_p = std::make_unique<viz::CopyOutputResult>(
                format, destination, gfx::Rect(), false);
            return true;
          }

          auto releaser = data.TakeReleaser<
              mojo::PendingRemote<viz::mojom::TextureReleaser>>();
          if (!releaser)
            return false;  // Illegal to provide texture without Releaser.

          // Returns a result with a ReleaseCallback that will return here and
          // proxy the callback over mojo to the CopyOutputResult's origin via a
          // mojo::Remote<viz::mojom::TextureReleaser> remote.
          viz::CopyOutputResult::ReleaseCallbacks release_callbacks;
          release_callbacks.emplace_back(
              base::BindOnce(&Release, std::move(releaser)));

          *out_p = std::make_unique<viz::CopyOutputTextureResult>(
              viz::CopyOutputResult::Format::RGBA, rect,
              viz::CopyOutputResult::TextureResult(*mailbox, *color_space),
              std::move(release_callbacks));
          return true;
        }
      }

    case viz::CopyOutputResult::Format::I420_PLANES:
    case viz::CopyOutputResult::Format::NV12:
      break;  // Not intended for transport across service boundaries.
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
