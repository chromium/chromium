// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_DEDUPING_FACTORY_H__
#define EXTENSIONS_BROWSER_API_DECLARATIVE_DEDUPING_FACTORY_H__

#include <stddef.h>

#include <list>
#include <string>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"

namespace extensions {

// Factory class that stores a cache of the last |N| created objects of each
// kind. These objects need to be immutable, refcounted objects that are derived
// from BaseClassT. The objects do not need to be RefCountedThreadSafe. If a new
// instance of an object is created that is identical to a pre-existing object,
// it is discarded and the pre-existing object is recycled.
//
// BaseClassT needs to provide a comparison operations. Like the following:
//
// class BaseClassT {
//   virtual bool Equals(const BaseClassT* other) const;
// };
//
// The unit test shows an example.
template <typename BaseClassT, typename ValueT>
class DedupingFactory {
 public:
  // Factory methods for BaseClass instances. |value| contains e.g. the json
  // dictionary that describes the object to be instantiated. |error| is used
  // to return error messages in case the extension passed an action that was
  // syntactically correct but semantically incorrect. |bad_message| is set to
  // true in case |dict| does not confirm to the validated JSON specification.
  typedef scoped_refptr<const BaseClassT> (*FactoryMethod)(
      const std::string& instance_type,
      ValueT /* value */,
      std::string* /* error */,
      bool* /* bad_message */);

  enum Parameterized {
    // Two instantiated objects may be different and we need to check for
    // equality to see whether we can recycle one.
    IS_PARAMETERIZED,
    // The objects are not parameterized, i.e. all created instances are the
    // same and it is sufficient to create a single one.
    IS_NOT_PARAMETERIZED
  };

  // Creates a DedupingFactory with a MRU cache of size |max_number_prototypes|
  // per instance_type. If we find a match within the cache, the factory reuses
  // that instance instead of creating a new one. The cache size should not be
  // too large because we probe linearly whether an element is in the cache.
  explicit DedupingFactory(size_t max_number_prototypes);

  DedupingFactory(const DedupingFactory&) = delete;
  DedupingFactory& operator=(const DedupingFactory&) = delete;

  ~DedupingFactory();

  void RegisterFactoryMethod(const std::string& instance_type,
                             Parameterized parameterized,
                             FactoryMethod factory_method);

  scoped_refptr<const BaseClassT> Instantiate(const std::string& instance_type,
                                              ValueT value,
                                              std::string* error,
                                              bool* bad_message);

  void ClearPrototypes();

 private:
  typedef std::string InstanceType;
  // Cache of previous prototypes in most-recently-used order. Most recently
  // used objects are at the end.
  using PrototypeList = std::list<scoped_refptr<const BaseClassT>>;
  using ExistingPrototypes = base::flat_map<InstanceType, PrototypeList>;
  using FactoryMethods = base::flat_map<InstanceType, FactoryMethod>;
  using ParameterizedTypes = base::flat_set<InstanceType>;

  const size_t max_number_prototypes_;
  ExistingPrototypes prototypes_;
  FactoryMethods factory_methods_;
  ParameterizedTypes parameterized_types_;
};

template <typename BaseClassT, typename ValueT>
DedupingFactory<BaseClassT, ValueT>::DedupingFactory(
    size_t max_number_prototypes)
    : max_number_prototypes_(max_number_prototypes) {}

template <typename BaseClassT, typename ValueT>
DedupingFactory<BaseClassT, ValueT>::~DedupingFactory() {}

template <typename BaseClassT, typename ValueT>
void DedupingFactory<BaseClassT, ValueT>::RegisterFactoryMethod(
    const std::string& instance_type,
    typename DedupingFactory<BaseClassT, ValueT>::Parameterized parameterized,
    FactoryMethod factory_method) {
  DCHECK(!base::Contains(factory_methods_, instance_type));
  factory_methods_[instance_type] = factory_method;
  if (parameterized == IS_PARAMETERIZED) {
    parameterized_types_.insert(instance_type);
  }
}

template <typename BaseClassT, typename ValueT>
scoped_refptr<const BaseClassT>
DedupingFactory<BaseClassT, ValueT>::Instantiate(
    const std::string& instance_type,
    ValueT value,
    std::string* error,
    bool* bad_message) {
  typename FactoryMethods::const_iterator factory_method_iter =
      factory_methods_.find(instance_type);
  if (factory_method_iter == factory_methods_.end()) {
    *error = "Invalid instance type " + instance_type;
    *bad_message = true;
    return scoped_refptr<const BaseClassT>();
  }

  FactoryMethod factory_method = factory_method_iter->second;

  PrototypeList& prototypes = prototypes_[instance_type];

  // We can take a shortcut for objects that are not parameterized. For those
  // only a single instance may ever exist so we can simplify the creation
  // logic.
  if (!base::Contains(parameterized_types_, instance_type)) {
    if (prototypes.empty()) {
      scoped_refptr<const BaseClassT> new_object =
          (*factory_method)(instance_type, value, error, bad_message);
      if (!new_object.get() || !error->empty() || *bad_message) {
        return scoped_refptr<const BaseClassT>();
      }
      prototypes.push_back(new_object);
    }
    return prototypes.front();
  }

  // Handle parameterized objects.
  scoped_refptr<const BaseClassT> new_object =
      (*factory_method)(instance_type, value, error, bad_message);
  if (!new_object.get() || !error->empty() || *bad_message) {
    return scoped_refptr<const BaseClassT>();
  }

  size_t length = 0;
  for (typename PrototypeList::iterator i = prototypes.begin();
       i != prototypes.end();
       ++i) {
    if ((*i)->Equals(new_object.get())) {
      // Move the old object to the end of the queue so that it gets
      // discarded later.
      scoped_refptr<const BaseClassT> old_object = *i;
      prototypes.erase(i);
      prototypes.push_back(old_object);
      return old_object;
    }
    ++length;
  }

  if (length >= max_number_prototypes_)
    prototypes.pop_front();
  prototypes.push_back(new_object);

  return new_object;
}

template <typename BaseClassT, typename ValueT>
void DedupingFactory<BaseClassT, ValueT>::ClearPrototypes() {
  prototypes_.clear();
}

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_DEDUPING_FACTORY_H__
