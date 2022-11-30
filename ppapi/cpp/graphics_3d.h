// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_GRAPHICS_3D_H_
#define PPAPI_CPP_GRAPHICS_3D_H_

#include <stdint.h>

#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the API to create a 3D rendering context in the browser.
namespace pp {

class CompletionCallback;
class InstanceHandle;

/// This class represents a 3D rendering context in the browser.
class Graphics3D : public Resource {
 public:
  /// Default constructor for creating an is_null() Graphics3D object.
  Graphics3D();

  /// A constructor for creating and initializing a 3D rendering context.
  /// The returned context is created off-screen and must be attached
  /// to a module instance using <code>Instance::BindGraphics</code> to draw on
  /// the web page.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  ///
  /// @param[in] attrib_list The list of attributes (name=value pairs) for the
  /// context. The list is terminated with
  /// <code>PP_GRAPHICS3DATTRIB_NONE</code>. The <code>attrib_list</code> may
  /// be <code>NULL</code> or empty (first attribute is
  /// <code>PP_GRAPHICS3DATTRIB_NONE</code>). If an attribute is not specified
  /// in <code>attrib_list</code>, then the default value is used.
  ///
  /// Attributes are classified into two categories:
  ///
  /// 1. AtLeast: The attribute value in the returned context will meet or
  ///            exceed the value requested when creating the object.
  /// 2. Exact: The attribute value in the returned context is equal to
  ///          the value requested when creating the object.
  ///
  /// AtLeast attributes are (all have default values of 0):
  ///
  /// <code>PP_GRAPHICS3DATTRIB_ALPHA_SIZE</code>
  /// <code>PP_GRAPHICS3DATTRIB_BLUE_SIZE</code>
  /// <code>PP_GRAPHICS3DATTRIB_GREEN_SIZE</code>
  /// <code>PP_GRAPHICS3DATTRIB_RED_SIZE</code>
  /// <code>PP_GRAPHICS3DATTRIB_DEPTH_SIZE</code>
  /// <code>PP_GRAPHICS3DATTRIB_STENCIL_SIZE</code>
  /// <code>PP_GRAPHICS3DATTRIB_SAMPLES</code>
  /// <code>PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS</code>
  ///
  /// Exact attributes are:
  ///
  /// <code>PP_GRAPHICS3DATTRIB_WIDTH</code> Default 0
  /// <code>PP_GRAPHICS3DATTRIB_HEIGHT</code> Default 0
  /// <code>PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR</code>
  /// Default: Implementation defined.
  ///
  /// On failure, the object will be is_null().
  Graphics3D(const InstanceHandle& instance,
             const int32_t attrib_list[]);

  /// A constructor for creating and initializing a 3D rendering context. The
  /// returned context is created off-screen. It must be attached to a
  /// module instance using <code>Instance::BindGraphics</code> to draw on the
  /// web page.
  ///
  /// This constructor is identical to the 2-argument version except that this
  /// version allows sharing of resources with another context.
  ///
  /// @param[in] instance The instance that will own the new Graphics3D.
  ///
  /// @param[in] share_context Specifies the context with which all
  /// shareable data will be shared. The shareable data is defined by the
  /// client API (note that for OpenGL and OpenGL ES, shareable data excludes
  /// texture objects named 0). An arbitrary number of Graphics3D resources
  /// can share data in this fashion.
  //
  /// @param[in] attrib_list The list of attributes for the context. See the
  /// 2-argument version of this constructor for more information.
  ///
  /// On failure, the object will be is_null().
  Graphics3D(const InstanceHandle& instance,
             const Graphics3D& share_context,
             const int32_t attrib_list[]);

  /// Destructor.
  ~Graphics3D();

  /// GetAttribs() retrieves the value for each attribute in
  /// <code>attrib_list</code>. The list has the same structure as described
  /// for the constructor. All attribute values specified in
  /// <code>pp_graphics_3d.h</code> can be retrieved.
  ///
  /// @param[in,out] attrib_list The list of attributes (name=value pairs) for
  /// the context. The list is terminated with
  /// <code>PP_GRAPHICS3DATTRIB_NONE</code>.
  ///
  /// The following error codes may be returned on failure:
  ///
  /// PP_ERROR_BADRESOURCE if context is invalid.
  /// PP_ERROR_BADARGUMENT if <code>attrib_list</code> is NULL or any attribute
  /// in the <code>attrib_list</code> is not a valid attribute.
  ///
  /// <strong>Example:</strong>
  ///
  /// @code
  /// int attrib_list[] = {PP_GRAPHICS3DATTRIB_RED_SIZE, 0,
  ///                      PP_GRAPHICS3DATTRIB_GREEN_SIZE, 0,
  ///                      PP_GRAPHICS3DATTRIB_BLUE_SIZE, 0,
  ///                      PP_GRAPHICS3DATTRIB_NONE};
  /// GetAttribs(context, attrib_list);
  /// int red_bits = attrib_list[1];
  /// int green_bits = attrib_list[3];
  /// int blue_bits = attrib_list[5];
  /// @endcode
  ///
  /// This example retrieves the values for rgb bits in the color buffer.
  int32_t GetAttribs(int32_t attrib_list[]) const;

  /// SetAttribs() sets the values for each attribute in
  /// <code>attrib_list</code>. The list has the same structure as the list
  /// used in the constructors.
  ///
  /// Attributes that can be specified are:
  /// - PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR
  ///
  /// On failure the following error codes may be returned:
  /// - PP_ERROR_BADRESOURCE if context is invalid.
  /// - PP_ERROR_BADARGUMENT if attrib_list is NULL or any attribute in the
  ///   attrib_list is not a valid attribute.
  int32_t SetAttribs(const int32_t attrib_list[]);

  /// ResizeBuffers() resizes the backing surface for the context.
  ///
  /// @param[in] width The width of the backing surface.
  /// @param[in] height The height of the backing surface.
  ///
  /// @return An int32_t containing <code>PP_ERROR_BADRESOURCE</code> if
  /// context is invalid or <code>PP_ERROR_BADARGUMENT</code> if the value
  /// specified for width or height is less than zero.
  /// <code>PP_ERROR_NOMEMORY</code> might be returned on the next
  /// SwapBuffers() callback if the surface could not be resized due to
  /// insufficient resources.
  int32_t ResizeBuffers(int32_t width, int32_t height);

  /// SwapBuffers() makes the contents of the color buffer available for
  /// compositing. This function has no effect on off-screen surfaces: surfaces
  /// not bound to any module instance. The contents of ancillary buffers are
  /// always undefined after calling SwapBuffers(). The contents of the color
  /// buffer are undefined if the value of the
  /// <code>PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR</code> attribute of context is
  /// not <code>PP_GRAPHICS3DATTRIB_BUFFER_PRESERVED</code>.
  ///
  /// SwapBuffers() runs in asynchronous mode. Specify a callback function and
  /// the argument for that callback function. The callback function will be
  /// executed on the calling thread after the color buffer has been composited
  /// with rest of the html page. While you are waiting for a SwapBuffers()
  /// callback, additional calls to SwapBuffers() will fail.
  ///
  /// Because the callback is executed (or thread unblocked) only when the
  /// instance's current state is actually on the screen, this function
  /// provides a way to rate limit animations. By waiting until the image is on
  /// the screen before painting the next frame, you can ensure you're not
  /// generating updates faster than the screen can be updated.
  ///
  /// SwapBuffers() performs an implicit flush operation on context.
  /// If the context gets into an unrecoverable error condition while
  /// processing a command, the error code will be returned as the argument
  /// for the callback. The callback may return the following error codes:
  ///
  /// <code>PP_ERROR_NOMEMORY</code>
  /// <code>PP_ERROR_CONTEXT_LOST</code>
  ///
  /// Note that the same error code may also be obtained by calling GetError().
  ///
  /// param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of SwapBuffers().
  ///
  /// @return An int32_t containing <code>PP_ERROR_BADRESOURCE</code> if
  /// context is invalid or <code>PP_ERROR_BADARGUMENT</code> if callback is
  /// invalid.
  int32_t SwapBuffers(const CompletionCallback& cc);
};

}  // namespace pp

#endif  // PPAPI_CPP_GRAPHICS_3D_H_
