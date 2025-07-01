/* Copyright 2010 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPB_VAR_DEPRECATED_H_
#define PPAPI_C_DEV_PPB_VAR_DEPRECATED_H_

#include "ppapi/c/dev/deprecated_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

struct PPP_Class_Deprecated;

#define PPB_VAR_DEPRECATED_INTERFACE_0_3 "PPB_Var(Deprecated);0.3"
#define PPB_VAR_DEPRECATED_INTERFACE PPB_VAR_DEPRECATED_INTERFACE_0_3

/**
 * @file
 * Defines the PPB_Var_Deprecated struct.
 * See http://code.google.com/p/ppapi/wiki/InterfacingWithJavaScript
 * for general information on using this interface.
 * {PENDING: Should the generated doc really be pointing to methods?}
 *
 * @addtogroup PPB
 * @{
 */

struct PPB_Var_Deprecated {
  /**
   * Adds a reference to the given var. If this is not a refcounted object,
   * this function will do nothing so you can always call it no matter what the
   * type.
   */
  void (*AddRef)(struct PP_Var var);

  /**
   * Removes a reference to given var, deleting it if the internal refcount
   * becomes 0. If the given var is not a refcounted object, this function will
   * do nothing so you can always call it no matter what the type.
   */
  void (*Release)(struct PP_Var var);

  /**
   * Creates a string var from a string. The string must be encoded in valid
   * UTF-8 and is NOT NULL-terminated, the length must be specified in |len|.
   * It is an error if the string is not valid UTF-8.
   *
   * If the length is 0, the |data| pointer will not be dereferenced and may
   * be NULL. Note, however, that if you do this, the "NULL-ness" will not be
   * preserved, as VarToUtf8 will never return NULL on success, even for empty
   * strings.
   *
   * The resulting object will be a refcounted string object. It will be
   * AddRef()ed for the caller. When the caller is done with it, it should be
   * Release()d.
   *
   * On error (basically out of memory to allocate the string, or input that
   * is not valid UTF-8), this function will return a Null var.
   */
  struct PP_Var (*VarFromUtf8)(PP_Module module,
                               const char* data, uint32_t len);

  /**
   * Converts a string-type var to a char* encoded in UTF-8. This string is NOT
   * NULL-terminated. The length will be placed in |*len|. If the string is
   * valid but empty the return value will be non-NULL, but |*len| will still
   * be 0.
   *
   * If the var is not a string, this function will return NULL and |*len| will
   * be 0.
   *
   * The returned buffer will be valid as long as the underlying var is alive.
   * If the plugin frees its reference, the string will be freed and the pointer
   * will be to random memory.
   */
  const char* (*VarToUtf8)(struct PP_Var var, uint32_t* len);

  /**
   * Returns true if the property with the given name exists on the given
   * object, false if it does not. Methods are also counted as properties.
   *
   * The name can either be a string or an integer var. It is an error to pass
   * another type of var as the name.
   *
   * If you pass an invalid name or object, the exception will be set (if it is
   * non-NULL, and the return value will be false).
   */
  bool (*HasProperty)(struct PP_Var object,
                      struct PP_Var name,
                      struct PP_Var* exception);

  /**
   * Identical to HasProperty, except that HasMethod additionally checks if the
   * property is a function.
   */
  bool (*HasMethod)(struct PP_Var object,
                    struct PP_Var name,
                    struct PP_Var* exception);

  /**
   * Returns the value of the given property. If the property doesn't exist, the
   * exception (if non-NULL) will be set and a "Void" var will be returned.
   */
  struct PP_Var (*GetProperty)(struct PP_Var object,
                               struct PP_Var name,
                               struct PP_Var* exception);

  /**
   * Retrieves all property names on the given object. Property names include
   * methods.
   *
   * If there is a failure, the given exception will be set (if it is non-NULL).
   * On failure, |*properties| will be set to NULL and |*property_count| will be
   * set to 0.
   *
   * A pointer to the array of property names will be placesd in |*properties|.
   * The caller is responsible for calling Release() on each of these properties
   * (as per normal refcounted memory management) as well as freeing the array
   * pointer with PPB_Core.MemFree().
   *
   * This function returns all "enumerable" properties. Some JavaScript
   * properties are "hidden" and these properties won't be retrieved by this
   * function, yet you can still set and get them.
   *
   * Example:
   * <pre>  uint32_t count;
   *   PP_Var* properties;
   *   ppb_var.GetAllPropertyNames(object, &count, &properties);
   *
   *   ...use the properties here...
   *
   *   for (uint32_t i = 0; i < count; i++)
   *     ppb_var.Release(properties[i]);
   *   ppb_core.MemFree(properties); </pre>
   */
  void (*GetAllPropertyNames)(struct PP_Var object,
                              uint32_t* property_count,
                              struct PP_Var** properties,
                              struct PP_Var* exception);

  /**
   * Sets the property with the given name on the given object. The exception
   * will be set, if it is non-NULL, on failure.
   */
  void (*SetProperty)(struct PP_Var object,
                      struct PP_Var name,
                      struct PP_Var value,
                      struct PP_Var* exception);

  /**
   * Removes the given property from the given object. The property name must
   * be an string or integer var, using other types will throw an exception
   * (assuming the exception pointer is non-NULL).
   */
  void (*RemoveProperty)(struct PP_Var object,
                         struct PP_Var name,
                         struct PP_Var* exception);

  // TODO(brettw) need native array access here.

  /**
   * Invoke the function |method_name| on the given object. If |method_name|
   * is a Null var, the default method will be invoked, which is how you can
   * invoke function objects.
   *
   * Unless it is type Null, |method_name| must be a string. Unlike other
   * Var functions, integer lookup is not supported since you can't call
   * functions on integers in JavaScript.
   *
   * Pass the arguments to the function in order in the |argv| array, and the
   * number of arguments in the |argc| parameter. |argv| can be NULL if |argc|
   * is zero.
   *
   * Example:
   *   Call(obj, VarFromUtf8("DoIt"), 0, NULL, NULL) = obj.DoIt() in JavaScript.
   *   Call(obj, PP_MakeNull(), 0, NULL, NULL) = obj() in JavaScript.
   */
  struct PP_Var (*Call)(struct PP_Var object,
                        struct PP_Var method_name,
                        uint32_t argc,
                        struct PP_Var* argv,
                        struct PP_Var* exception);

  /**
   * Invoke the object as a constructor.
   *
   * For example, if |object| is |String|, this is like saying |new String| in
   * JavaScript.
   */
  struct PP_Var (*Construct)(struct PP_Var object,
                             uint32_t argc,
                             struct PP_Var* argv,
                             struct PP_Var* exception);

  /**
   * If the object is an instance of the given class, then this method returns
   * true and sets *object_data to the value passed to CreateObject provided
   * object_data is non-NULL. Otherwise, this method returns false.
   */
  bool (*IsInstanceOf)(struct PP_Var var,
                       const struct PPP_Class_Deprecated* object_class,
                       void** object_data);

  /**
   * Creates an object that the plugin implements. The plugin supplies a
   * pointer to the class interface it implements for that object, and its
   * associated internal data that represents that object. This object data
   * must be unique among all "live" objects.
   *
   * The returned object will have a reference count of 1. When the reference
   * count reached 0, the class' Destruct function wlil be called.
   *
   * On failure, this will return a null var. This probably means the module
   * was invalid.
   *
   * Example: Say we're implementing a "Point" object.
   * <pre>  void PointDestruct(void* object) {
   *     delete (Point*)object;
   *   }
   *
   *   const PPP_Class_Deprecated point_class = {
   *     ... all the other class functions go here ...
   *     &PointDestruct
   *   };
   *
   *    * The plugin's internal object associated with the point.
   *   class Point {
   *     ...
   *   };
   *
   *   PP_Var MakePoint(int x, int y) {
   *     return CreateObject(&point_class, new Point(x, y));
   *   }</pre>
   */
  struct PP_Var (*CreateObject)(PP_Instance instance,
                                const struct PPP_Class_Deprecated* object_class,
                                void* object_data);

  // Like CreateObject but takes a module. This will be deleted when all callers
  // can be changed to use the PP_Instance CreateObject one.
  struct PP_Var (*CreateObjectWithModuleDeprecated)(
      PP_Module module,
      const struct PPP_Class_Deprecated* object_class,
      void* object_data);
};

/**
 * @}
 * End addtogroup PPB
 */
#endif  // PPAPI_C_DEV_PPB_VAR_DEPRECATED_H_

