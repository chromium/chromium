// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_OBJECT_MANAGER_H_
#define DBUS_OBJECT_MANAGER_H_

#include <stdint.h>

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

// Newer D-Bus services implement the Object Manager interface to inform other
// clients about the objects they export, the properties of those objects, and
// notification of changes in the set of available objects:
//     http://dbus.freedesktop.org/doc/dbus-specification.html
//       #standard-interfaces-objectmanager
//
// This interface is very closely tied to the Properties interface, and uses
// even more levels of nested dictionaries and variants. In addition to
// simplifying implementation, since there tends to be a single object manager
// per service, spanning the complete set of objects an interfaces available,
// the classes implemented here make dealing with this interface simpler.
//
// Except where noted, use of this class replaces the need for the code
// documented in dbus/property.h
//
// Client implementation classes should begin by deriving from the
// dbus::ObjectManager::Interface class, and defining a Properties structure as
// documented in dbus/property.h.
//
// Example:
//   class ExampleClient : public dbus::ObjectManager::Interface {
//    public:
//     struct Properties : public dbus::PropertySet {
//       dbus::Property<std::string> name;
//       dbus::Property<uint16_t> version;
//       dbus::Property<dbus::ObjectPath> parent;
//       dbus::Property<std::vector<std::string>> children;
//
//       Properties(dbus::ObjectProxy* object_proxy,
//                  const PropertyChangedCallback callback)
//           : dbus::PropertySet(object_proxy, kExampleInterface, callback) {
//         RegisterProperty("Name", &name);
//         RegisterProperty("Version", &version);
//         RegisterProperty("Parent", &parent);
//         RegisterProperty("Children", &children);
//       }
//       virtual ~Properties() {}
//     };
//
// The link between the implementation class and the object manager is set up
// in the constructor and removed in the destructor; the class should maintain
// a pointer to its object manager for use in other methods and establish
// itself as the implementation class for its interface.
//
// Example:
//   explicit ExampleClient::ExampleClient(dbus::Bus* bus)
//       : bus_(bus),
//         weak_ptr_factory_(this) {
//     object_manager_ = bus_->GetObjectManager(kServiceName, kManagerPath);
//     object_manager_->RegisterInterface(kInterface, this);
//   }
//
//   virtual ExampleClient::~ExampleClient() {
//     object_manager_->UnregisterInterface(kInterface);
//   }
//
// This class calls GetManagedObjects() asynchronously after the remote service
// becomes available and additionally refreshes managed objects after the
// service stops or restarts.
//
// The object manager interface class has one abstract method that must be
// implemented by the class to create Properties structures on demand. As well
// as implementing this, you will want to implement a public GetProperties()
// method.
//
// Example:
//   dbus::PropertySet* CreateProperties(dbus::ObjectProxy* object_proxy,
//                                       const std::string& interface_name)
//       override {
//     Properties* properties = new Properties(
//           object_proxy, interface_name,
//           base::BindRepeating(&PropertyChanged,
//                               weak_ptr_factory_.GetWeakPtr(),
//                               object_path));
//     return static_cast<dbus::PropertySet*>(properties);
//   }
//
//   Properties* GetProperties(const dbus::ObjectPath& object_path) {
//     return static_cast<Properties*>(
//         object_manager_->GetProperties(object_path, kInterface));
//   }
//
// Note that unlike classes that only use dbus/property.h there is no need
// to connect signals or obtain the initial values of properties. The object
// manager class handles that for you.
//
// PropertyChanged is a method of your own to notify your observers of a change
// in your properties, either as a result of a signal from the Properties
// interface or from the Object Manager interface. You may also wish to
// implement the optional ObjectAdded and ObjectRemoved methods of the class
// to likewise notify observers.
//
// When your class needs an object proxy for a given object path, it may
// obtain it from the object manager. Unlike the equivalent method on the bus
// this will return NULL if the object is not known.
//
//   object_proxy = object_manager_->GetObjectProxy(object_path);
//   if (object_proxy) {
//     ...
//   }
//
// There is no need for code using your implementation class to be aware of the
// use of object manager behind the scenes, the rules for updating properties
// documented in dbus/property.h still apply.

namespace dbus {

const char kObjectManagerInterface[] = "org.freedesktop.DBus.ObjectManager";
const char kObjectManagerGetManagedObjects[] = "GetManagedObjects";
const char kObjectManagerInterfacesAdded[] = "InterfacesAdded";
const char kObjectManagerInterfacesRemoved[] = "InterfacesRemoved";

class Bus;
class MessageReader;
class ObjectProxy;
class Response;
class Signal;

// ObjectManager implements both the D-Bus client components of the D-Bus
// Object Manager interface, as internal methods, and a public API for
// client classes to utilize.
class CHROME_DBUS_EXPORT ObjectManager final
    : public base::RefCountedThreadSafe<ObjectManager> {
 public:
  // ObjectManager::Interface must be implemented by any class wishing to have
  // its remote objects managed by an ObjectManager.
  class Interface {
   public:
    virtual ~Interface() {}

    // Called by ObjectManager to create a Properties structure for the remote
    // D-Bus object identified by |object_path| and accessibile through
    // |object_proxy|. The D-Bus interface name |interface_name| is that passed
    // to RegisterInterface() by the implementation class.
    //
    // The implementation class should create and return an instance of its own
    // subclass of dbus::PropertySet; ObjectManager will then connect signals
    // and update the properties from its own internal message reader.
    virtual PropertySet* CreateProperties(
        ObjectProxy *object_proxy,
        const dbus::ObjectPath& object_path,
        const std::string& interface_name) = 0;

    // Called by ObjectManager to inform the implementation class that an
    // object has been added with the path |object_path|. The D-Bus interface
    // name |interface_name| is that passed to RegisterInterface() by the
    // implementation class.
    //
    // If a new object implements multiple interfaces, this method will be
    // called on each interface implementation with differing values of
    // |interface_name| as appropriate. An implementation class will only
    // receive multiple calls if it has registered for multiple interfaces.
    virtual void ObjectAdded(const ObjectPath& object_path,
                             const std::string& interface_name) { }

    // Called by ObjectManager to inform the implementation class than an
    // object with the path |object_path| has been removed. Ths D-Bus interface
    // name |interface_name| is that passed to RegisterInterface() by the
    // implementation class. Multiple interfaces are handled as with
    // ObjectAdded().
    //
    // This method will be called before the Properties structure and the
    // ObjectProxy object for the given interface are cleaned up, it is safe
    // to retrieve them during removal to vary processing.
    virtual void ObjectRemoved(const ObjectPath& object_path,
                               const std::string& interface_name) { }
  };

  // Client code should use Bus::GetObjectManager() instead of this constructor.
  static scoped_refptr<ObjectManager> Create(Bus* bus,
                                             const std::string& service_name,
                                             const ObjectPath& object_path);

  ObjectManager(const ObjectManager&) = delete;
  ObjectManager& operator=(const ObjectManager&) = delete;

  // Register a client implementation class |interface| for the given D-Bus
  // interface named in |interface_name|. That object's CreateProperties()
  // method will be used to create instances of dbus::PropertySet* when
  // required.
  void RegisterInterface(const std::string& interface_name,
                         Interface* interface);

  // Unregister the implementation class for the D-Bus interface named in
  // |interface_name|, objects and properties of this interface will be
  // ignored.
  void UnregisterInterface(const std::string& interface_name);

  // Checks whether an interface is registered.
  bool IsInterfaceRegisteredForTesting(const std::string& interface_name) const;

  // Returns a list of object paths, in an undefined order, of objects known
  // to this manager.
  std::vector<ObjectPath> GetObjects();

  // Returns the list of object paths, in an undefined order, of objects
  // implementing the interface named in |interface_name| known to this manager.
  std::vector<ObjectPath> GetObjectsWithInterface(
      const std::string& interface_name);

  // Returns a ObjectProxy pointer for the given |object_path|. Unlike
  // the equivalent method on Bus this will return NULL if the object
  // manager has not been informed of that object's existence.
  ObjectProxy* GetObjectProxy(const ObjectPath& object_path);

  // Returns a PropertySet* pointer for the given |object_path| and
  // |interface_name|, or NULL if the object manager has not been informed of
  // that object's existence or the interface's properties. The caller should
  // cast the returned pointer to the appropriate type, e.g.:
  //   static_cast<Properties*>(GetProperties(object_path, my_interface));
  PropertySet* GetProperties(const ObjectPath& object_path,
                             const std::string& interface_name);

  // Instructs the object manager to refresh its list of managed objects;
  // automatically called by the D-Bus thread manager, there should never be
  // a need to call this manually.
  void GetManagedObjects();

  // Cleans up any match rules and filter functions added by this ObjectManager.
  // The Bus object will take care of this so you don't have to do it manually.
  //
  // BLOCKING CALL.
  void CleanUp();

 private:
  friend class base::RefCountedThreadSafe<ObjectManager>;

  ObjectManager(Bus* bus,
                const std::string& service_name,
                const ObjectPath& object_path);
  ~ObjectManager();

  // Called from the constructor to add a match rule for PropertiesChanged
  // signals on the D-Bus thread and set up a corresponding filter function.
  bool SetupMatchRuleAndFilter();

  // Called on the origin thread once the match rule and filter have been set
  // up. Connects the InterfacesAdded and InterfacesRemoved signals and
  // refreshes objects if the service is available. |success| is false if an
  // error occurred during setup and true otherwise.
  void OnSetupMatchRuleAndFilterComplete(bool success);

  // Called by dbus:: when a message is received. This is used to filter
  // PropertiesChanged signals from the correct sender and relay the event to
  // the correct PropertySet.
  static DBusHandlerResult HandleMessageThunk(DBusConnection* connection,
                                              DBusMessage* raw_message,
                                              void* user_data);
  DBusHandlerResult HandleMessage(DBusConnection* connection,
                                  DBusMessage* raw_message);

  // Called when a PropertiesChanged signal is received from the sender.
  // This method notifies the relevant PropertySet that it should update its
  // properties based on the received signal. Called from HandleMessage.
  void NotifyPropertiesChanged(const dbus::ObjectPath object_path,
                               Signal* signal);
  void NotifyPropertiesChangedHelper(const dbus::ObjectPath object_path,
                                     Signal* signal);

  // Called by dbus:: in response to the GetManagedObjects() method call.
  void OnGetManagedObjects(Response* response);

  // Called by dbus:: when an InterfacesAdded signal is received and initially
  // connected.
  void InterfacesAddedReceived(Signal* signal);
  void InterfacesAddedConnected(const std::string& interface_name,
                                const std::string& signal_name,
                                bool success);

  // Called by dbus:: when an InterfacesRemoved signal is received and
  // initially connected.
  void InterfacesRemovedReceived(Signal* signal);
  void InterfacesRemovedConnected(const std::string& interface_name,
                                  const std::string& signal_name,
                                  bool success);

  // Updates the map entry for the object with path |object_path| using the
  // D-Bus message in |reader|, which should consist of an dictionary mapping
  // interface names to properties dictionaries as recieved by both the
  // GetManagedObjects() method return and the InterfacesAdded() signal.
  void UpdateObject(const ObjectPath& object_path, MessageReader* reader);

  // Updates the properties structure of the object with path |object_path|
  // for the interface named |interface_name| using the D-Bus message in
  // |reader| which should consist of the properties dictionary for that
  // interface.
  //
  // Called by UpdateObjects() for each interface in the dictionary; this
  // method takes care of both creating the entry in the ObjectMap and
  // ObjectProxy if required, as well as the PropertySet instance for that
  // interface if necessary.
  void AddInterface(const ObjectPath& object_path,
                    const std::string& interface_name,
                    MessageReader* reader);

  // Removes the properties structure of the object with path |object_path|
  // for the interfaces named |interface_name|.
  //
  // If no further interfaces remain, the entry in the ObjectMap is discarded.
  void RemoveInterface(const ObjectPath& object_path,
                       const std::string& interface_name);

  // Removes all objects and interfaces from the object manager when
  // |old_owner| is not the empty string and/or re-requests the set of managed
  // objects when |new_owner| is not the empty string.
  void NameOwnerChanged(const std::string& old_owner,
                        const std::string& new_owner);

  // Write |new_owner| to |service_name_owner_|. This method makes sure write
  // happens on the DBus thread, which is the sole writer to
  // |service_name_owner_|.
  void UpdateServiceNameOwner(const std::string& new_owner);

  // Valid in between the constructor and `CleanUp()`.
  // After Cleanup(), `this` lifetime might exceed Bus's one.
  raw_ptr<Bus> bus_;
  std::string service_name_;
  std::string service_name_owner_;
  std::string match_rule_;
  ObjectPath object_path_;
  raw_ptr<ObjectProxy, AcrossTasksDanglingUntriaged> object_proxy_;
  bool setup_success_ = false;
  bool cleanup_called_ = false;

  // Maps the name of an interface to the implementation class used for
  // instantiating PropertySet structures for that interface's properties.
  typedef std::map<std::string, raw_ptr<Interface, CtnExperimental>>
      InterfaceMap;
  InterfaceMap interface_map_;

  // Each managed object consists of a ObjectProxy used to make calls
  // against that object and a collection of D-Bus interface names and their
  // associated PropertySet structures.
  struct Object {
    Object();
    ~Object();

    raw_ptr<ObjectProxy, AcrossTasksDanglingUntriaged> object_proxy;

    // Maps the name of an interface to the specific PropertySet structure
    // of that interface's properties.
    typedef std::map<const std::string, raw_ptr<PropertySet, CtnExperimental>>
        PropertiesMap;
    PropertiesMap properties_map;
  };

  // Maps the object path of an object to the Object structure.
  typedef std::map<const ObjectPath, raw_ptr<Object, CtnExperimental>>
      ObjectMap;
  ObjectMap object_map_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ObjectManager> weak_ptr_factory_{this};
};

}  // namespace dbus

#endif  // DBUS_OBJECT_MANAGER_H_
