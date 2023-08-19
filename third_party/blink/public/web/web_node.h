/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NODE_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NODE_H_

#include "cc/paint/element_id.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "v8/include/v8-forward.h"

namespace blink {

class Node;
class WebDocument;
class WebElement;
class WebElementCollection;
class WebPluginContainer;

// Provides access to some properties of a DOM node.
// Note that the class design requires that neither this class nor any of its
// subclasses have any virtual methods (other than the destructor), so that it
// is possible to safely static_cast an instance of one class to the appropriate
// subclass based on the actual type of the wrapped blink::Node. For the same
// reason, subclasses must not add any additional data members.
class BLINK_EXPORT WebNode {
 public:
  virtual ~WebNode();

  WebNode();
  WebNode(const WebNode&);
  WebNode& operator=(const WebNode&);

  void Reset();
  void Assign(const WebNode&);

  bool Equals(const WebNode&) const;
  // Required for using WebNodes in std maps.  Note the order used is
  // arbitrary and should not be expected to have any specific meaning.
  bool LessThan(const WebNode&) const;

  bool IsNull() const;

  WebNode ParentNode() const;
  WebString NodeValue() const;
  WebDocument GetDocument() const;
  WebNode FirstChild() const;
  WebNode LastChild() const;
  WebNode PreviousSibling() const;
  WebNode NextSibling() const;

  bool IsLink() const;
  bool IsDocumentNode() const;
  bool IsDocumentTypeNode() const;
  bool IsCommentNode() const;
  bool IsTextNode() const;
  bool IsFocusable() const;
  bool IsContentEditable() const;
  bool IsElementNode() const;
  void SimulateClick();

  // See cc/paint/element_id.h for the definition of these ids.
  cc::ElementId ScrollingElementIdForTesting() const;

  // The argument should be lower-cased.
  WebElementCollection GetElementsByHTMLTagName(const WebString&) const;

  // https://developer.mozilla.org/en-US/docs/Web/API/Document/querySelector
  // If the JS API would have thrown this returns null instead.
  WebElement QuerySelector(const WebString& selector) const;

  WebVector<WebElement> QuerySelectorAll(const WebString& selector) const;

  bool Focused() const;

  WebPluginContainer* PluginContainer() const;

  bool IsInsideFocusableElementOrARIAWidget() const;

  v8::Local<v8::Value> ToV8Value(v8::Local<v8::Object> creation_context,
                                 v8::Isolate*);

  int GetDevToolsNodeId() const;

  // Helper to downcast to `T`. Will fail with a CHECK() if converting to `T` is
  // not legal. The returned `T` will always be non-null if `this` is non-null.
  template <typename T>
  T To() const;

  // Helper to downcast to `T`, returning a null `T` if the conversion could not
  // be performed.
  template <typename T>
  T DynamicTo() const;

#if INSIDE_BLINK
  WebNode(Node*);
  WebNode& operator=(Node*);
  operator Node*() const;

  template <typename T>
  T* Unwrap() {
    return static_cast<T*>(private_.Get());
  }

  template <typename T>
  const T* ConstUnwrap() const {
    return static_cast<const T*>(private_.Get());
  }
#endif

 protected:
  WebPrivatePtrForGC<Node> private_;
};

#define DECLARE_WEB_NODE_TYPE_CASTS(type)      \
  template <>                                  \
  BLINK_EXPORT type WebNode::To<type>() const; \
  template <>                                  \
  BLINK_EXPORT type WebNode::DynamicTo<type>() const

#if INSIDE_BLINK
#define DEFINE_WEB_NODE_TYPE_CASTS(type, predicate)    \
  template <>                                          \
  BLINK_EXPORT type WebNode::To<type>() const {        \
    SECURITY_CHECK(IsNull() || (predicate));           \
    type result;                                       \
    result.WebNode::Assign(*this);                     \
    return result;                                     \
  }                                                    \
  template <>                                          \
  BLINK_EXPORT type WebNode::DynamicTo<type>() const { \
    type result;                                       \
    if (!IsNull() && (predicate)) {                    \
      result.WebNode::Assign(*this);                   \
    }                                                  \
    return result;                                     \
  }
#endif

inline bool operator==(const WebNode& a, const WebNode& b) {
  return a.Equals(b);
}

inline bool operator!=(const WebNode& a, const WebNode& b) {
  return !(a == b);
}

inline bool operator<(const WebNode& a, const WebNode& b) {
  return a.LessThan(b);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NODE_H_
