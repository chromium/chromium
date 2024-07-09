# Advanced Metadata Definitions and Usages

The core documentation for Views Metadata and properties usage can be found
[here](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h?q=%22Properties%20-%22). For most instances, this is sufficient to add metadata to any
given view descendant class.

There are, however, some cases where extra care is needed to properly add
metadata to a class.

[TOC]


## Nested or Private Classes

In some cases a View sub-class is declared as a nested class of varying
visibility. In order to properly attach metadata to such a class the following
should be used. Consider the following example class:

In the header it is declared thusly:


```
class ASH_EXPORT MyViewClass : public views::View {
 public:
  METADATA_HEADER(MyViewClass, views::View);

  // ... Public API goes here ...

 private:
  // ... A private nested view ...
  class MyNestedView;
};
```


Then in the .cc file:


```
class MyViewClass::MyNestedView : public views::View {
 public:
  METADATA_HEADER(MyNestedView, views::View);

 // ... Public API goes here …

};
```


Then, to properly declare the metadata for `MyViewClass::MyNestedView` along
with any necessary properties, the `BEGIN_METADATA` macro takes an extra
parameter to define the outer class or scope:


```
BEGIN_METADATA(MyViewClass, MyNestedView)
END_METADATA
```


This ensures the internal metadata classes are defined with the properly
nested scope. The rest of the definition remains the same as the above linked
document comment.



## Read-only Properties

While this may seem strange that a property would be read-only, it’s a good
debugging tool by allowing the ui-devtools to peer into the object instance for
more insight about it’s internal state. In those cases, you may want to define
such a property in the metadata. A read-only property only has a “getter” (see
the [core documentation](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h?q=%22Properties%20-%22) about how to define a “getter”). It is as simple
as using a different macro when defining the class’ metadata.


```
ADD_READONLY_PROPERTY_METADATA(bool, IsDrawn);
```


In this case, the `IsDrawn` property is calculated and not something that can
be directly set. However, it is likely useful during debugging to see whether
the view considers itself to be drawn. Sometimes you will need to refactor the
“getter” to properly match the naming requirements.


## Custom Type-converters for POD (Plain Old Data) Types

While this may be rare, there are cases where a type has been declared as a
mere alias to a POD type such as *int*, *unsigned*, *float*, etc. In most
circumstances the default type converter which will convert to/from a string
using the natural string format is sufficient. However in some cases this type
alias is meant to convey that this POD type is to be treated differently and
thus needs to convert to/from strings using a different syntax. Unfortunately,
C++ doesn’t have the concept of making a truly uniquely identifiable type from
a POD type. In those cases some extra care is needed when defining a custom
type converter for such a type.

Consider the following type:

```
using datetime_t = double;
```

If a property were using that type, the compiler would select the following
type converter:


```
template <>
struct TypeConverter<double> {
  //...
};
```


This is most likely not what was desired since the likely intent is to allow
the use of a date/time string to display or set the value. There is no way to
force the compiler to select a `TypeConverter<datetime_t> `specialization. We
need to do a little extra work.

First of all, make the type unique with the following macro:


```
MAKE_TYPE_UNIQUE(SkColor);
```


Then use the `UNIQUE_TYPE_NAME` macro to wrap the name you want to be unique.


```
template <>
struct VIEWS_EXPORT TypeConverter<UNIQUE_TYPE_NAME(datetime_t)>
    : BaseTypeConverter<true, false> {
  // Define implementations for ToString and FromString here. See type_conversion.h
};
```


It’s also helpful to define your own type-alias to later reference.


```
using DateTimeConverter = TypeConverter<UNIQUE_TYPE_NAME(datetime_t)>;
```


Finally, you need to be explicit about when to select that type converter when
defining the property in the metadata. Add the reference to the type converter
specialization to the macro.


```
BEGIN_METADATA(MyDateTimeView, views::View)
ADD_PROPERTY_METADATA(datetime_t, CurrentDateTime, DateTimeConverter)
END_METADATA
```


**NOTE:** SkColor is such a case where the above technique has been used. For
properties of that type, use the SkColorConverter type alias when defining the
metadata for a property of that type.


```
ADD_PROPERTY_METADATA(SkColor, BackgroundColor, SkColorConverter)
```

## Non-Serializable Types

Due to their very nature, some property types may not be readily convertible to
or from strings. Common instances of this are pointers or a `unique_ptr<T>`.
However, knowing that *something* has been set for such a property can be
helpful. Unlike the read-only properties described above, a property of a
non-serializable type may still have a “getter” and a “setter”. Within the code
at run-time, getting or setting the value directly via the getter and setter is
perfectly valid.

The type converter for a non-serializable type may return something from the
ToString() method, but it will typically return std::nullopt from the
FromString() method. For a non-serializable type, the ui-devtools front-end
won’t call the setter since whatever “value” it has is presumed to be
unconvertable to a valid value.

For pointers and `unique_ptr<T>` a partial specialization is already available
in which the compiler should already select. Most non-serializable types are
already unique, so creating a type converter specialization is relatively
straightforward. Otherwise see the above section to resolve this.

For other instances that may need this you will need to define your own type
converter specialization. This is done the same as other type converter
specializations, except for the ancestor specialization. The ancestor
`BaseTypeConverter` template takes a few `bool` parameters. The first of which
indicates whether a property type is serializable.
