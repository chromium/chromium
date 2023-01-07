// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_IMAGE_DATA_H_
#define PPAPI_CPP_IMAGE_DATA_H_

#include <stdint.h>

#include "ppapi/c/ppb_image_data.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/size.h"

/// @file
/// This file defines the APIs for determining how a browser
/// handles image data.
namespace pp {

class InstanceHandle;

class ImageData : public Resource {
 public:
  /// Default constructor for creating an is_null() <code>ImageData</code>
  /// object.
  ImageData();

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has already been reference counted.
  ///
  /// @param[in] resource A PP_Resource corresponding to image data.
  ImageData(PassRef, PP_Resource resource);

  /// The copy constructor for <code>ImageData</code>. This constructor
  /// produces an <code>ImageData</code> object that shares the underlying
  /// <code>Image</code> resource with <code>other</code>.
  ///
  /// @param[in] other A pointer to an image data.
  ImageData(const ImageData& other);

  /// A constructor that allocates a new <code>ImageData</code> in the browser
  /// with the provided parameters. The resulting object will be is_null() if
  /// the allocation failed.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  ///
  /// @param[in] format A PP_ImageDataFormat containing desired image format.
  /// PP_ImageDataFormat is an enumeration of the different types of
  /// image data formats. Refer to
  /// <a href="../c/ppb__image__data_8h.html">
  /// <code>ppb_image_data.h</code></a> for further information.
  ///
  /// @param[in] size A pointer to a <code>Size</code> containing the image
  /// size.
  ///
  /// @param[in] init_to_zero A bool used to determine transparency at
  /// creation. Set the <code>init_to_zero</code> flag if you want the bitmap
  /// initialized to transparent during the creation process. If this flag is
  /// not set, the current contents of the bitmap will be undefined, and the
  /// module should be sure to set all the pixels.
  ImageData(const InstanceHandle& instance,
            PP_ImageDataFormat format,
            const Size& size,
            bool init_to_zero);

  /// This function decrements the reference count of this
  /// <code>ImageData</code> and increments the reference count of the
  /// <code>other</code> <code>ImageData</code>. This <code>ImageData</code>
  /// shares the underlying image resource with <code>other</code>.
  ///
  /// @param[in] other An other image data.
  ///
  /// @return A new image data context.
  ImageData& operator=(const ImageData& other);

  /// IsImageDataFormatSupported() returns <code>true</code> if the supplied
  /// format is supported by the browser. Note:
  /// <code>PP_IMAGEDATAFORMAT_BGRA_PREMUL</code> and
  /// <code>PP_IMAGEDATAFORMAT_RGBA_PREMUL</code> formats are always supported.
  /// Other image formats do not make this guarantee, and should be checked
  /// first with IsImageDataFormatSupported() before using.
  ///
  /// @param[in] format Image data format.
  ///
  /// @return <code>true</code> if the format is supported by the browser.
  static bool IsImageDataFormatSupported(PP_ImageDataFormat format);

  /// GetNativeImageDataFormat() determines the browser's preferred format for
  /// images. Using this format guarantees no extra conversions will occur when
  /// painting.
  ///
  /// @return <code>PP_ImageDataFormat</code> containing the preferred format.
  static PP_ImageDataFormat GetNativeImageDataFormat();

  /// A getter function for returning the current format for images.
  ///
  /// @return <code>PP_ImageDataFormat</code> containing the preferred format.
  PP_ImageDataFormat format() const { return desc_.format; }

  /// A getter function for returning the image size.
  ///
  /// @return The image size in pixels.
  pp::Size size() const { return desc_.size; }

  /// A getter function for returning the row width in bytes.
  ///
  /// @return The row width in bytes.
  int32_t stride() const { return desc_.stride; }

  /// A getter function for returning a raw pointer to the image pixels.
  ///
  /// @return A raw pointer to the image pixels.
  void* data() const { return data_; }

  /// This function is used retrieve the address of the given pixel for 32-bit
  /// pixel formats.
  ///
  /// @param[in] coord A <code>Point</code> representing the x and y
  /// coordinates for a specific pixel.
  ///
  /// @return The address for the pixel.
  const uint32_t* GetAddr32(const Point& coord) const;

  /// This function is used retrieve the address of the given pixel for 32-bit
  /// pixel formats.
  ///
  /// @param[in] coord A <code>Point</code> representing the x and y
  /// coordinates for a specific pixel.
  ///
  /// @return The address for the pixel.
  uint32_t* GetAddr32(const Point& coord);

 private:
  void InitData();

  PP_ImageDataDesc desc_;
  void* data_;
};

}  // namespace pp

#endif  // PPAPI_CPP_IMAGE_DATA_H_
