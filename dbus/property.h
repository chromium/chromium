// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_PROPERTY_H_
#define DBUS_PROPERTY_H_

#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "dbus/dbus_export.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

// D-Bus objects frequently provide sets of properties accessed via a
// standard interface of method calls and signals to obtain the current value,
// set a new value and be notified of changes to the value. Unfortunately this
// interface makes heavy use of variants and dictionaries of variants. The
// classes defined here make dealing with properties in a type-safe manner
// possible.
//
// Client implementation classes should define a Properties structure, deriving
// from the PropertySet class defined here. This structure should contain a
// member for each property defined as an instance of the Property<> class,
// specifying the type to the template. Finally the structure should chain up
// to the PropertySet constructor, and then call RegisterProperty() for each
// property defined to associate them with their string name.
//
// Example:
//   class ExampleClient {
//    public:
//     struct Properties : public dbus::PropertySet {
//       dbus::Property<std::string> name;
//       dbus::Property<uint16_t> version;
//       dbus::Property<dbus::ObjectPath> parent;
//       dbus::Property<std::vector<std::string>> children;
//
//       Properties(dbus::ObjectProxy* object_proxy,
//                  const PropertyChangedCallback callback)
//           : dbus::PropertySet(object_proxy, "com.example.DBus", callback) {
//         RegisterProperty("Name", &name);
//         RegisterProperty("Version", &version);
//         RegisterProperty("Parent", &parent);
//         RegisterProperty("Children", &children);
//       }
//       virtual ~Properties() {}
//     };
//
// The Properties structure requires a pointer to the object proxy of the
// actual object to track, and after construction should have signals
// connected to that object and initial values set by calling ConnectSignals()
// and GetAll(). The structure should not outlive the object proxy, so it
// is recommended that the lifecycle of both be managed together.
//
// Example (continued):
//
//     typedef std::map<std::pair<dbus::ObjectProxy*, Properties*>> Object;
//     typedef std::map<dbus::ObjectPath, Object> ObjectMap;
//     ObjectMap object_map_;
//
//     dbus::ObjectProxy* GetObjectProxy(const dbus::ObjectPath& object_path) {
//       return GetObject(object_path).first;
//     }
//
//     Properties* GetProperties(const dbus::ObjectPath& object_path) {
//       return GetObject(object_path).second;
//     }
//
//     Object GetObject(const dbus::ObjectPath& object_path) {
//       ObjectMap::iterator it = object_map_.find(object_path);
//       if (it != object_map_.end())
//         return it->second;
//
//       dbus::ObjectProxy* object_proxy = bus->GetObjectProxy(...);
//       // connect signals, etc.
//
//       Properties* properties = new Properties(
//           object_proxy,
//           base::BindRepeating(&PropertyChanged,
//                               weak_ptr_factory_.GetWeakPtr(),
//                               object_path));
//       properties->ConnectSignals();
//       properties->GetAll();
//
//       Object object = std::make_pair(object_proxy, properties);
//       object_map_[object_path] = object;
//       return object;
//     }
//  };
//
// This now allows code using the client implementation to access properties
// in a type-safe manner, and assuming the PropertyChanged callback is
// propagated up to observers, be notified of changes. A typical access of
// the current value of the name property would be:
//
//   ExampleClient::Properties* p = example_client->GetProperties(object_path);
//   std::string name = p->name.value();
//
// Normally these values are updated from signals emitted by the remote object,
// in case an explicit round-trip is needed to obtain the current value, the
// Get() method can be used and indicates whether or not the value update was
// successful. The updated value can be obtained in the callback using the
// value() method.
//
//   p->children.Get(base::BindOnce(&OnGetChildren));
//
// A new value can be set using the Set() method, the callback indicates
// success only; it is up to the remote object when (and indeed if) it updates
// the property value, and whether it emits a signal or a Get() call is
// required to obtain it.
//
//   p->version.Set(20, base::BindOnce(&OnSetVersion))

namespace dbus {

// D-Bus Properties interface constants, declared here rather than
// in property.cc because template methods use them.
const char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";
const char kPropertiesGetAll[] = "GetAll";
const char kPropertiesGet[] = "Get";
const char kPropertiesSet[] = "Set";
const char kPropertiesChanged[] = "PropertiesChanged";

class PropertySet;

// PropertyBase is an abstract base-class consisting of the parts of
// the Property<> template that are not type-specific, such as the
// associated PropertySet, property name, and the type-unsafe parts
// used by PropertySet.
class CHROME_DBUS_EXPORT PropertyBase {
 public:
  PropertyBase();

  PropertyBase(const PropertyBase&) = delete;
  PropertyBase& operator=(const PropertyBase&) = delete;

  virtual ~PropertyBase();

  // Initializes the |property_set| and property |name| so that method
  // calls may be made from this class. This method is called by
  // PropertySet::RegisterProperty() passing |this| for |property_set| so
  // there should be no need to call it directly. If you do beware that
  // no ownership or reference to |property_set| is taken so that object
  // must outlive this one.
  void Init(PropertySet* property_set, const std::string& name);

  // Retrieves the name of this property, this may be useful in observers
  // to avoid specifying the name in more than once place, e.g.
  //
  //   void Client::PropertyChanged(const dbus::ObjectPath& object_path,
  //                                const std::string &property_name) {
  //     Properties& properties = GetProperties(object_path);
  //     if (property_name == properties.version.name()) {
  //       // Handle version property changing
  //     }
  //   }
  const std::string& name() const { return name_; }

  // Returns true if property is valid, false otherwise.
  bool is_valid() const { return is_valid_; }

  // Allows to mark Property as valid or invalid.
  void set_valid(bool is_valid) { is_valid_ = is_valid; }

  // Method used by PropertySet to retrieve the value from a MessageReader,
  // no knowledge of the contained type is required, this method returns
  // true if its expected type was found, false if not.
  // Implementation provided by specialization.
  virtual bool PopValueFromReader(MessageReader* reader) = 0;

  // Method used by PropertySet to append the set value to a MessageWriter,
  // no knowledge of the contained type is required.
  // Implementation provided by specialization.
  virtual void AppendSetValueToWriter(MessageWriter* writer) = 0;

  // Method used by test and stub implementations of dbus::PropertySet::Set
  // to replace the property value with the set value without using a
  // dbus::MessageReader.
  virtual void ReplaceValueWithSetValue() = 0;

 protected:
  // Retrieves the associated property set.
  PropertySet* property_set() { return property_set_; }

 private:
  // Pointer to the PropertySet instance that this instance is a member of,
  // no ownership is taken and |property_set_| must outlive this class.
  raw_ptr<PropertySet> property_set_;

  bool is_valid_;

  // Name of the property.
  std::string name_;
};

// PropertySet groups a collection of properties for a remote object
// together into a single structure, fixing their types and name such
// that calls made through it are type-safe.
//
// Clients always sub-class this to add the properties, and should always
// provide a constructor that chains up to this and then calls
// RegisterProperty() for each property defined.
//
// After creation, client code should call ConnectSignals() and most likely
// GetAll() to seed initial values and update as changes occur.
class CHROME_DBUS_EXPORT PropertySet {
 public:
  // Callback for changes to cached values of properties, either notified
  // via signal, or as a result of calls to Get() and GetAll(). The |name|
  // argument specifies the name of the property changed.
  using PropertyChangedCallback =
      base::RepeatingCallback<void(const std::string& name)>;

  // Constructs a property set, where |object_proxy| specifies the proxy for
  // the/ remote object that these properties are for, care should be taken to
  // ensure that this object does not outlive the lifetime of the proxy;
  // |interface| specifies the D-Bus interface of these properties, and
  // |property_changed_callback| specifies the callback for when properties
  // are changed, this may be a NULL callback.
  PropertySet(ObjectProxy* object_proxy, const std::string& interface,
              const PropertyChangedCallback& property_changed_callback);

  PropertySet(const PropertySet&) = delete;
  PropertySet& operator=(const PropertySet&) = delete;

  // Destructor; we don't hold on to any references or memory that needs
  // explicit clean-up, but clang thinks we might.
  virtual ~PropertySet();

  // Registers a property, generally called from the subclass constructor;
  // pass the |name| of the property as used in method calls and signals,
  // and the pointer to the |property| member of the structure. This will
  // call the PropertyBase::Init method.
  void RegisterProperty(const std::string& name, PropertyBase* property);

  // Connects property change notification signals to the object, generally
  // called immediately after the object is created and before calls to other
  // methods. Sub-classes may override to use different D-Bus signals.
  virtual void ConnectSignals();

  // Methods connected by ConnectSignals() and called by dbus:: when
  // a property is changed. Sub-classes may override if the property
  // changed signal provides different arguments.
  virtual void ChangedReceived(Signal* signal);
  virtual void ChangedConnected(const std::string& interface_name,
                                const std::string& signal_name,
                                bool success);

  // Callback for Get() method, |success| indicates whether or not the
  // value could be retrived, if true the new value can be obtained by
  // calling value() on the property.
  using GetCallback = base::OnceCallback<void(bool success)>;

  // Requests an updated value from the remote object for |property|
  // incurring a round-trip. |callback| will be called when the new
  // value is available. This may not be implemented by some interfaces,
  // and may be overriden by sub-classes if interfaces use different
  // method calls.
  virtual void Get(PropertyBase* property, GetCallback callback);
  virtual void OnGet(PropertyBase* property, GetCallback callback,
                     Response* response);

  // The synchronous version of Get().
  // This should never be used on an interactive thread.
  virtual bool GetAndBlock(PropertyBase* property);

  // Queries the remote object for values of all properties and updates
  // initial values. Sub-classes may override to use a different D-Bus
  // method, or if the remote object does not support retrieving all
  // properties, either ignore or obtain each property value individually.
  virtual void GetAll();
  virtual void OnGetAll(Response* response);

  // Callback for Set() method, |success| indicates whether or not the
  // new property value was accepted by the remote object.
  using SetCallback = base::OnceCallback<void(bool success)>;

  // Requests that the remote object for |property| change the property to
  // its new value. |callback| will be called to indicate the success or
  // failure of the request, however the new value may not be available
  // depending on the remote object. This method may be overridden by
  // sub-classes if interfaces use different method calls.
  virtual void Set(PropertyBase* property, SetCallback callback);
  virtual void OnSet(PropertyBase* property, SetCallback callback,
                     Response* response);

  // The synchronous version of Set().
  // This should never be used on an interactive thread.
  virtual bool SetAndBlock(PropertyBase* property);

  // Update properties by reading an array of dictionary entries, each
  // containing a string with the name and a variant with the value, from
  // |message_reader|. Returns false if message is in incorrect format.
  bool UpdatePropertiesFromReader(MessageReader* reader);

  // Updates a single property by reading a string with the name and a
  // variant with the value from |message_reader|. Returns false if message
  // is in incorrect format, or property type doesn't match.
  bool UpdatePropertyFromReader(MessageReader* reader);

  // Calls the property changed callback passed to the constructor, used
  // by sub-classes that do not call UpdatePropertiesFromReader() or
  // UpdatePropertyFromReader(). Takes the |name| of the changed property.
  void NotifyPropertyChanged(const std::string& name);

  // Retrieves the object proxy this property set was initialized with,
  // provided for sub-classes overriding methods that make D-Bus calls
  // and for Property<>. Not permitted with const references to this class.
  ObjectProxy* object_proxy() { return object_proxy_; }

  // Retrieves the interface of this property set.
  const std::string& interface() const { return interface_; }

 protected:
  // Get a weak pointer to this property set, provided so that sub-classes
  // overriding methods that make D-Bus calls may use the existing (or
  // override) callbacks without providing their own weak pointer factory.
  base::WeakPtr<PropertySet> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Invalidates properties by reading an array of names, from
  // |message_reader|. Returns false if message is in incorrect format.
  bool InvalidatePropertiesFromReader(MessageReader* reader);

  // Pointer to object proxy for making method calls, no ownership is taken
  // so this must outlive this class.
  raw_ptr<ObjectProxy, AcrossTasksDanglingUntriaged> object_proxy_;

  // Interface of property, e.g. "org.chromium.ExampleService", this is
  // distinct from the interface of the method call itself which is the
  // general D-Bus Properties interface "org.freedesktop.DBus.Properties".
  std::string interface_;

  // Callback for property changes.
  PropertyChangedCallback property_changed_callback_;

  // Map of properties (as PropertyBase*) defined in the structure to
  // names as used in D-Bus method calls and signals. The base pointer
  // restricts property access via this map to type-unsafe and non-specific
  // actions only.
  typedef std::map<const std::string, raw_ptr<PropertyBase, CtnExperimental>>
      PropertiesMap;
  PropertiesMap properties_map_;

  // Weak pointer factory as D-Bus callbacks may last longer than these
  // objects.
  base::WeakPtrFactory<PropertySet> weak_ptr_factory_{this};
};

// Property template, this defines the type-specific and type-safe methods
// of properties that can be accessed as members of a PropertySet structure.
//
// Properties provide a cached value that has an initial sensible default
// until the reply to PropertySet::GetAll() is retrieved and is updated by
// all calls to that method, PropertySet::Get() and property changed signals
// also handled by PropertySet. It can be obtained by calling value() on the
// property.
//
// It is recommended that this cached value be used where necessary, with
// code using PropertySet::PropertyChangedCallback to be notified of changes,
// rather than incurring a round-trip to the remote object for each property
// access.
//
// Where a round-trip is necessary, the Get() method is provided. And to
// update the remote object value, the Set() method is also provided; these
// both simply call methods on PropertySet.
//
// Handling of particular D-Bus types is performed via specialization,
// typically the PopValueFromReader() and AppendSetValueToWriter() methods
// will need to be provided, and in rare cases a constructor to provide a
// default value. Specializations for basic D-Bus types, strings, object
// paths and arrays are provided for you.
template <class T>
class CHROME_DBUS_EXPORT Property : public PropertyBase {
 public:
  Property() {}
  ~Property() override {}

  // Retrieves the cached value.
  const T& value() const { return value_; }

  // Requests an updated value from the remote object incurring a
  // round-trip. |callback| will be called when the new value is available.
  // This may not be implemented by some interfaces.
  virtual void Get(dbus::PropertySet::GetCallback callback) {
    property_set()->Get(this, std::move(callback));
  }

  // The synchronous version of Get().
  // This should never be used on an interactive thread.
  virtual bool GetAndBlock() {
    return property_set()->GetAndBlock(this);
  }

  // Requests that the remote object change the property value to |value|,
  // |callback| will be called to indicate the success or failure of the
  // request, however the new value may not be available depending on the
  // remote object.
  virtual void Set(const T& value, dbus::PropertySet::SetCallback callback) {
    set_value_ = value;
    property_set()->Set(this, std::move(callback));
  }

  // The synchronous version of Set().
  // This should never be used on an interactive thread.
  virtual bool SetAndBlock(const T& value) {
    set_value_ = value;
    return property_set()->SetAndBlock(this);
  }

  // Method used by PropertySet to retrieve the value from a MessageReader,
  // no knowledge of the contained type is required, this method returns
  // true if its expected type was found, false if not.
  bool PopValueFromReader(MessageReader* reader) override;

  // Method used by PropertySet to append the set value to a MessageWriter,
  // no knowledge of the contained type is required.
  // Implementation provided by specialization.
  void AppendSetValueToWriter(MessageWriter* writer) override;

  // Method used by test and stub implementations of dbus::PropertySet::Set
  // to replace the property value with the set value without using a
  // dbus::MessageReader.
  void ReplaceValueWithSetValue() override {
    value_ = set_value_;
    property_set()->NotifyPropertyChanged(name());
  }

  // Method used by test and stub implementations to directly set the
  // value of a property.
  void ReplaceValue(const T& value) {
    value_ = value;
    property_set()->NotifyPropertyChanged(name());
  }

  // Method used by test and stub implementations to directly set the
  // |set_value_| of a property.
  void ReplaceSetValueForTesting(const T& value) { set_value_ = value; }

  // Method used by test and stub implementations to retrieve the |set_value|
  // of a property.
  const T& GetSetValueForTesting() const { return set_value_; }

 private:
  // Current cached value of the property.
  T value_;

  // Replacement value of the property.
  T set_value_;
};

// Clang and GCC don't agree on how attributes should work for explicitly
// instantiated templates. GCC ignores attributes on explicit instantiations
// (and emits a warning) while Clang requires the visiblity attribute on the
// explicit instantiations for them to be visible to other compilation units.
// Hopefully clang and GCC agree one day, and this can be cleaned up:
// https://llvm.org/bugs/show_bug.cgi?id=24815
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"

template <>
CHROME_DBUS_EXPORT Property<uint8_t>::Property();
template <>
CHROME_DBUS_EXPORT bool Property<uint8_t>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void Property<uint8_t>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT Property<uint8_t>;

template <>
CHROME_DBUS_EXPORT Property<bool>::Property();
template <>
CHROME_DBUS_EXPORT bool Property<bool>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void Property<bool>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT Property<bool>;

template <>
CHROME_DBUS_EXPORT Property<int16_t>::Property();
template <>
CHROME_DBUS_EXPORT bool Property<int16_t>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void Property<int16_t>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT Property<int16_t>;

template <>
CHROME_DBUS_EXPORT Property<uint16_t>::Property();
template <>
CHROME_DBUS_EXPORT bool Property<uint16_t>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void Property<uint16_t>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT Property<uint16_t>;

template <>
CHROME_DBUS_EXPORT Property<int32_t>::Property();
template <>
CHROME_DBUS_EXPORT bool Property<int32_t>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void Property<int32_t>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT Property<int32_t>;

template <>
CHROME_DBUS_EXPORT Property<uint32_t>::Property();
template <>
CHROME_DBUS_EXPORT bool Property<uint32_t>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void Property<uint32_t>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT Property<uint32_t>;

template <>
CHROME_DBUS_EXPORT Property<int64_t>::Property();
template <>
CHROME_DBUS_EXPORT bool Property<int64_t>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void Property<int64_t>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT Property<int64_t>;

template <>
CHROME_DBUS_EXPORT Property<uint64_t>::Property();
template <>
CHROME_DBUS_EXPORT bool Property<uint64_t>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void Property<uint64_t>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT Property<uint64_t>;

template <>
CHROME_DBUS_EXPORT Property<double>::Property();
template <>
CHROME_DBUS_EXPORT bool Property<double>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void Property<double>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT Property<double>;

template <>
CHROME_DBUS_EXPORT bool Property<std::string>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void Property<std::string>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT Property<std::string>;

template <>
CHROME_DBUS_EXPORT bool Property<ObjectPath>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void Property<ObjectPath>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT Property<ObjectPath>;

template <>
CHROME_DBUS_EXPORT bool Property<std::vector<std::string>>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void Property<
    std::vector<std::string>>::AppendSetValueToWriter(MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT Property<std::vector<std::string>>;

template <>
CHROME_DBUS_EXPORT bool Property<std::vector<ObjectPath>>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void Property<
    std::vector<ObjectPath>>::AppendSetValueToWriter(MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT Property<std::vector<ObjectPath>>;

template <>
CHROME_DBUS_EXPORT bool Property<std::vector<uint8_t>>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void Property<std::vector<uint8_t>>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT Property<std::vector<uint8_t>>;

template <>
CHROME_DBUS_EXPORT bool
Property<std::map<std::string, std::string>>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void
Property<std::map<std::string, std::string>>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT
    Property<std::map<std::string, std::string>>;

template <>
CHROME_DBUS_EXPORT bool
Property<std::vector<std::pair<std::vector<uint8_t>, uint16_t>>>::
    PopValueFromReader(MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void
Property<std::vector<std::pair<std::vector<uint8_t>, uint16_t>>>::
    AppendSetValueToWriter(MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT
    Property<std::vector<std::pair<std::vector<uint8_t>, uint16_t>>>;

template <>
CHROME_DBUS_EXPORT bool
Property<std::map<std::string, std::vector<uint8_t>>>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void
Property<std::map<std::string, std::vector<uint8_t>>>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT
    Property<std::map<std::string, std::vector<uint8_t>>>;

template <>
CHROME_DBUS_EXPORT bool
Property<std::map<uint16_t, std::vector<uint8_t>>>::PopValueFromReader(
    MessageReader* reader);
template <>
CHROME_DBUS_EXPORT void
Property<std::map<uint16_t, std::vector<uint8_t>>>::AppendSetValueToWriter(
    MessageWriter* writer);
extern template class CHROME_DBUS_EXPORT
    Property<std::map<uint16_t, std::vector<uint8_t>>>;

#pragma GCC diagnostic pop

}  // namespace dbus

#endif  // DBUS_PROPERTY_H_
