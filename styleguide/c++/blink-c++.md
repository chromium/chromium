# Blink C++ Style Guide

This document is a list of differences from the overall
[Chromium Style Guide](c++.md), which is in turn a set of differences from the
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html). The
long-term goal is to make both Chromium and Blink style more similar to Google
style over time, so this document aims to highlight areas where Blink style
differs from Chromium style.

[TOC]

## Prefer WTF types over STL and base types

See [Blink readme](../../third_party/blink/renderer/README.md#Type-dependencies)
for more details on Blink directories and their type usage.

**Good:**
```c++
  String title;
  Vector<KURL> urls;
  HashMap<int, Deque<RefPtr<SecurityOrigin>>> origins;
```

**Bad:**
```c++
  std::string title;
  std::vector<GURL> urls;
  std::unordered_map<int, std::deque<url::Origin>> origins;
```

When interacting with WTF types, use `wtf_size_t` instead of `size_t`.

## Do not use `new` and `delete`

Chromium [recommends avoiding bare new/delete](c++-dos-and-donts.md#use-and-instead-of-bare);
Blink bans them.  In addition to the options there, Blink objects may be
allocated using `blink::MakeGarbageCollected` and manage their lifetimes using
strong Blink GC references, depending on the type. See
[How Blink Works](https://docs.google.com/document/d/1aitSOucL0VHZa9Z2vbRJSyAIsAz24kX8LFByQ5xQnUg/edit#heading=h.ekwf97my4bgf)
for more information.

## Don't mix Create() factory methods and public constructors in one class.

A class only can have either Create() factory functions or public constructors.
In case you want to call MakeGarbageCollected<> from a Create() method, a
PassKey pattern can be used. Note that the auto-generated binding classes keep
using Create() methods consistently.

**Good:**
```c++
class HistoryItem {
 public:
  HistoryItem();
  ~HistoryItem();
  ...
}

void DocumentLoader::SetHistoryItemStateForCommit() {
  history_item_ = MakeGarbageCollected<HistoryItem>();
  ...
}
```

**Good:**
```c++
class BodyStreamBuffer {
 public:
  using PassKey = base::PassKey<BodyStreamBuffer>;
  static BodyStreamBuffer* Create();

  BodyStreamBuffer(PassKey);
  ...
}

BodyStreamBuffer* BodyStreamBuffer::Create() {
  auto* buffer = MakeGarbageCollected<BodyStreamBuffer>(PassKey());
  buffer->Init();
  return buffer;
}

BodyStreamBuffer::BodyStreamBuffer(PassKey) {}
```

**Bad:**
```c++
class HistoryItem {
 public:
  // Create() and a public constructor should not be mixed.
  static HistoryItem* Create() { return MakeGarbageCollected<HistoryItem>(); }

  HistoryItem();
  ~HistoryItem();
  ...
}
```

## Naming

### Use `CamelCase` for all function names

All function names should use `CamelCase()`-style names, beginning with an
uppercase letter.

As an exception, method names for web-exposed bindings begin with a lowercase
letter to match JavaScript.

**Good:**
```c++
class Document {
 public:
  // Function names should begin with an uppercase letter.
  virtual void Shutdown();

  // However, web-exposed function names should begin with a lowercase letter.
  LocalDOMWindow* defaultView();

  // ...
};
```

**Bad:**
```c++
class Document {
 public:
  // Though this is a getter, all Blink function names must use camel case.
  LocalFrame* frame() const { return frame_; }

  // ...
};
```

### Precede boolean values with words like “is” and “did”

**Good:**
```c++
bool is_valid;
bool did_send_data;
```

**Bad:**
```c++
bool valid;
bool sent_data;
```

### Precede setters with the word “Set”; use bare words for getters
Precede setters with the word “set”. Prefer bare words for getters. Setter and
getter names should match the names of the variable being accessed/mutated.

If a getter’s name collides with a type name, prefix it with “Get”.

**Good:**
```c++
class FrameTree {
 public:
  // Prefer to use the bare word for getters.
  Frame* FirstChild() const { return first_child_; }
  Frame* LastChild() const { return last_child_; }

  // However, if the type name and function name would conflict, prefix the
  // function name with “Get”.
  Frame* GetFrame() const { return frame_; }

  // ...
};
```

**Bad:**
```c++
class FrameTree {
 public:
  // Should not be prefixed with “Get” since there's no naming conflict.
  Frame* GetFirstChild() const { return first_child_; }
  Frame* GetLastChild() const { return last_child_; }

  // ...
};
```

### Precede getters that return values via out-arguments with the word “Get”
**Good:**
```c++
class RootInlineBox {
 public:
  Node* GetLogicalStartBoxWithNode(InlineBox*&) const;
  // ...
}
```

**Bad:**
```c++
class RootInlineBox {
 public:
  Node* LogicalStartBoxWithNode(InlineBox*&) const;
  // ...
}
```

### May leave obvious parameter names out of function declarations
The
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html#Function_Declarations_and_Definitions)
allows omitting parameter names out when they are unused. In Blink, you may
leave obvious parameter names out of function declarations for historical
reasons. A good rule of thumb is if the parameter type name contains the
parameter name (without trailing numbers or pluralization), then the parameter
name isn’t needed.

**Good:**
```c++
class Node {
 public:
  Node(TreeScope* tree_scope, ConstructionType construction_type);
  // You may leave them out like:
  // Node(TreeScope*, ConstructionType);

  // The function name makes the meaning of the parameters clear.
  void SetActive(bool);
  void SetDragged(bool);
  void SetHovered(bool);

  // Parameters are not obvious.
  DispatchEventResult DispatchDOMActivateEvent(int detail,
                                               Event& underlying_event);
};
```

**Bad:**
```c++
class Node {
 public:
  // ...

  // Parameters are not obvious.
  DispatchEventResult DispatchDOMActivateEvent(int, Event&);
};
```

## Prefer enums or StrongAliases to bare bools for function parameters
Prefer enums to bools for function parameters if callers are likely to be
passing constants, since named constants are easier to read at the call site.
Alternatively, you can use `base::StrongAlias<Tag, bool>`. An exception to this
rule is a setter function, where the name of the function already makes clear
what the boolean is.

**Good:**
```c++
class FrameLoader {
public:
  enum class CloseType {
    kNotForReload,
    kForReload,
  };

  bool ShouldClose(CloseType) {
    if (type == CloseType::kForReload) {
      ...
    } else {
      DCHECK_EQ(type, CloseType::kNotForReload);
      ...
    }
  }
};

// An named enum value makes it clear what the parameter is for.
if (frame_->Loader().ShouldClose(FrameLoader::CloseType::kNotForReload)) {
  // No need to use enums for boolean setters, since the meaning is clear.
  frame_->SetIsClosing(true);

  // ...
```

**Good:**
```c++
class FrameLoader {
public:
  using ForReload = base::StrongAlias<class ForReloadTag, bool>;

  bool ShouldClose(ForReload) {
    // A StrongAlias<_, bool> can be tested like a bool.
    if (for_reload) {
      ...
    } else {
      ...
    }
  }
};

// Using a StrongAlias makes it clear what the parameter is for.
if (frame_->Loader().ShouldClose(FrameLoader::ForReload(false))) {
  // No need to use enums for boolean setters, since the meaning is clear.
  frame_->SetIsClosing(true);

  // ...
```

**Bad:**
```c++
class FrameLoader {
public:
  bool ShouldClose(bool for_reload) {
    if (for_reload) {
      ...
    } else {
      ...
    }
  }
};

// Not obvious what false means here.
if (frame_->Loader().ShouldClose(false)) {
  frame_->SetIsClosing(ClosingState::kTrue);

  // ...
```

### Use `README.md` to document high-level components

Documentation for a related-set of classes and how they interact should be done
with a `README.md` file in the root directory of a component.
