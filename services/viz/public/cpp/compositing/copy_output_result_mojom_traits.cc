// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/copy_output_result_mojom_traits.h"

#include "base/bind.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {

// This class retains the SingleReleaseCallback of the CopyOutputResult that is
// being sent over mojo. A PendingRemote<TextureReleaser> that talks to this
// impl object will be sent over mojo instead of the release_callback_ (which is
// not serializable). Once the client calls Release, the release_callback_ will
// be called. An object of this class will remain alive until the MessagePipe
// attached to it goes away (i.e. StrongBinding is used).
class TextureReleaserImpl : public viz::mojom::TextureReleaser {
 public:
  explicit TextureReleaserImpl(
      std::unique_ptr<viz::SingleReleaseCallback> release_callback)
      : release_callback_(std::move(release_callback)) {}

  // mojom::TextureReleaser implementation:
  void Release(const gpu::SyncToken& sync_token, bool is_lost) override {
    release_callback_->Run(sync_token, is_lost);
  }

 private:
  std::unique_ptr<viz::SingleReleaseCallback> release_callback_;
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
    case viz::CopyOutputResult::Format::RGBA_BITMAP:
      return viz::mojom::CopyOutputResultFormat::RGBA_BITMAP;
    case viz::CopyOutputResult::Format::RGBA_TEXTURE:
      return viz::mojom::CopyOutputResultFormat::RGBA_TEXTURE;
    case viz::CopyOutputResult::Format::I420_PLANES:
      break;  // Not intended for transport across service boundaries.
  }
  NOTREACHED();
  return viz::mojom::CopyOutputResultFormat::RGBA_BITMAP;
}

// static
bool EnumTraits<viz::mojom::CopyOutputResultFormat,
                viz::CopyOutputResult::Format>::
    FromMojom(viz::mojom::CopyOutputResultFormat input,
              viz::CopyOutputResult::Format* out) {
  switch (input) {
    case viz::mojom::CopyOutputResultFormat::RGBA_BITMAP:
      *out = viz::CopyOutputResult::Format::RGBA_BITMAP;
      return true;
    case viz::mojom::CopyOutputResultFormat::RGBA_TEXTURE:
      *out = viz::CopyOutputResult::Format::RGBA_TEXTURE;
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
const gfx::Rect& StructTraits<viz::mojom::CopyOutputResultDataView,
                              std::unique_ptr<viz::CopyOutputResult>>::
    rect(const std::unique_ptr<viz::CopyOutputResult>& result) {
  return result->rect();
}

// static
const SkBitmap& StructTraits<viz::mojom::CopyOutputResultDataView,
                             std::unique_ptr<viz::CopyOutputResult>>::
    bitmap(const std::unique_ptr<viz::CopyOutputResult>& result) {
  // This will return a non-drawable bitmap if the result was not
  // RGBA_BITMAP or if the result is empty.
  return result->AsSkBitmap();
}

// static
base::Optional<gpu::Mailbox>
StructTraits<viz::mojom::CopyOutputResultDataView,
             std::unique_ptr<viz::CopyOutputResult>>::
    mailbox(const std::unique_ptr<viz::CopyOutputResult>& result) {
  if (result->format() != viz::CopyOutputResult::Format::RGBA_TEXTURE ||
      result->IsEmpty()) {
    return base::nullopt;
  }
  return result->GetTextureResult()->mailbox;
}

// static
base::Optional<gpu::SyncToken>
StructTraits<viz::mojom::CopyOutputResultDataView,
             std::unique_ptr<viz::CopyOutputResult>>::
    sync_token(const std::unique_ptr<viz::CopyOutputResult>& result) {
  if (result->format() != viz::CopyOutputResult::Format::RGBA_TEXTURE ||
      result->IsEmpty()) {
    return base::nullopt;
  }
  return result->GetTextureResult()->sync_token;
}

// static
base::Optional<gfx::ColorSpace>
StructTraits<viz::mojom::CopyOutputResultDataView,
             std::unique_ptr<viz::CopyOutputResult>>::
    color_space(const std::unique_ptr<viz::CopyOutputResult>& result) {
  if (result->format() != viz::CopyOutputResult::Format::RGBA_TEXTURE ||
      result->IsEmpty()) {
    return base::nullopt;
  }
  return result->GetTextureResult()->color_space;
}

// static
mojo::PendingRemote<viz::mojom::TextureReleaser>
StructTraits<viz::mojom::CopyOutputResultDataView,
             std::unique_ptr<viz::CopyOutputResult>>::
    releaser(const std::unique_ptr<viz::CopyOutputResult>& result) {
  if (result->format() != viz::CopyOutputResult::Format::RGBA_TEXTURE)
    return mojo::NullRemote();

  mojo::PendingRemote<viz::mojom::TextureReleaser> releaser;
  MakeSelfOwnedReceiver(
      std::make_unique<TextureReleaserImpl>(result->TakeTextureOwnership()),
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
  gfx::Rect rect;

  if (!data.ReadFormat(&format) || !data.ReadRect(&rect))
    return false;

  if (rect.IsEmpty()) {
    // An empty rect implies an empty result.
    *out_p = std::make_unique<viz::CopyOutputResult>(format, gfx::Rect());
    return true;
  }

  switch (format) {
    case viz::CopyOutputResult::Format::RGBA_BITMAP: {
      SkBitmap bitmap;
      if (!data.ReadBitmap(&bitmap) || !bitmap.readyToDraw())
        return false;

      *out_p = std::make_unique<viz::CopyOutputSkBitmapResult>(
          rect, std::move(bitmap));
      return true;
    }

    case viz::CopyOutputResult::Format::RGBA_TEXTURE: {
      base::Optional<gpu::Mailbox> mailbox;
      if (!data.ReadMailbox(&mailbox) || !mailbox)
        return false;
      base::Optional<gpu::SyncToken> sync_token;
      if (!data.ReadSyncToken(&sync_token) || !sync_token)
        return false;
      base::Optional<gfx::ColorSpace> color_space;
      if (!data.ReadColorSpace(&color_space) || !color_space)
        return false;

      if (mailbox->IsZero()) {
        // Returns an empty result.
        *out_p = std::make_unique<viz::CopyOutputResult>(
            viz::CopyOutputResult::Format::RGBA_TEXTURE, gfx::Rect());
        return true;
      }

      auto releaser =
          data.TakeReleaser<mojo::PendingRemote<viz::mojom::TextureReleaser>>();
      if (!releaser)
        return false;  // Illegal to provide texture without Releaser.

      // Returns a result with a SingleReleaseCallback that will return
      // here and proxy the callback over mojo to the CopyOutputResult's
      // origin via a mojo::Remote<viz::mojom::TextureReleaser> remote.
      *out_p = std::make_unique<viz::CopyOutputTextureResult>(
          rect, *mailbox, *sync_token, *color_space,
          viz::SingleReleaseCallback::Create(
              base::BindOnce(&Release, std::move(releaser))));
      return true;
    }

    case viz::CopyOutputResult::Format::I420_PLANES:
      break;  // Not intended for transport across service boundaries.
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo
