// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_SET_H_
#define EXTENSIONS_COMMON_EXTENSION_SET_H_

#include <iterator>
#include <map>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

class GURL;

namespace extensions {
class URLPatternSet;

// The one true extension container. Extensions are identified by their id.
// Only one extension can be in the set with a given ID.
class ExtensionSet {
 public:
  using ExtensionMap = std::map<ExtensionId, scoped_refptr<const Extension>>;

  // Iteration over the values of the map (given that it's an ExtensionSet,
  // it should iterate like a set iterator).
  class const_iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = scoped_refptr<const Extension>;
    using difference_type = std::ptrdiff_t;
    using pointer = scoped_refptr<const Extension>*;
    using reference = scoped_refptr<const Extension>&;

    const_iterator();
    const_iterator(const const_iterator& other);
    explicit const_iterator(ExtensionMap::const_iterator it);
    ~const_iterator();
    const_iterator& operator++() {
      ++it_;
      return *this;
    }
    const_iterator operator++(int) {
      const const_iterator old(*this);
      ++it_;
      return old;
    }
    const scoped_refptr<const Extension>& operator*() const {
      return it_->second;
    }
    const scoped_refptr<const Extension>* operator->() const {
      return &it_->second;
    }
    bool operator!=(const const_iterator& other) const {
      return it_ != other.it_;
    }
    bool operator==(const const_iterator& other) const {
      return it_ == other.it_;
    }

   private:
    ExtensionMap::const_iterator it_;
  };

  ExtensionSet();

  ExtensionSet(const ExtensionSet&) = delete;
  ExtensionSet(ExtensionSet&&);
  ExtensionSet& operator=(const ExtensionSet&) = delete;
  ExtensionSet& operator=(ExtensionSet&&) noexcept;

  ~ExtensionSet();

  size_t size() const { return extensions_.size(); }
  bool empty() const { return extensions_.empty(); }

  // Iteration support.
  const_iterator begin() const { return const_iterator(extensions_.begin()); }
  const_iterator end() const { return const_iterator(extensions_.end()); }

  // Returns true if the set contains the specified extension.
  bool Contains(const ExtensionId& id) const;

  // Adds the specified extension to the set. The set becomes an owner. Any
  // previous extension with the same ID is removed.
  // Returns true if there is no previous extension.
  bool Insert(const scoped_refptr<const Extension>& extension);

  // Copies different items from |extensions| to the current set and returns
  // whether anything changed.
  bool InsertAll(const ExtensionSet& extensions);

  // Removes the specified extension.
  // Returns true if the set contained the specified extension.
  bool Remove(const ExtensionId& id);

  // Removes all extensions.
  void Clear();

  // Returns the extension ID, or empty if none. This includes web URLs that
  // are part of an extension's web extent.
  ExtensionId GetExtensionOrAppIDByURL(const GURL& url) const;

  // Returns the Extension, or NULL if none.  This includes web URLs that are
  // part of an extension's web extent.
  // NOTE: This can return NULL if called before UpdateExtensions receives
  // bulk extension data (e.g. if called from
  // EventBindings::HandleContextCreated)
  const Extension* GetExtensionOrAppByURL(const GURL& url,
                                          bool include_guid = false) const;

  // Returns the app specified by the given |url|, if one exists. This will
  // return NULL if there is no entry with |url|, or if the extension with
  // |url| is not an app.
  const Extension* GetAppByURL(const GURL& url) const;

  // Returns the hosted app whose web extent contains the URL.
  const Extension* GetHostedAppByURL(const GURL& url) const;

  // Returns a hosted app that contains any URL that overlaps with the given
  // extent, if one exists.
  const Extension* GetHostedAppByOverlappingWebExtent(
      const URLPatternSet& extent) const;

  // Returns true if |new_url| is in the extent of the same extension as
  // |old_url|.  Also returns true if neither URL is in an app.
  bool InSameExtent(const GURL& old_url, const GURL& new_url) const;

  // Look up an Extension object by id or guid.
  const Extension* GetByID(const ExtensionId& id) const;
  const Extension* GetByGUID(const std::string& guid) const;
  const Extension* GetByIDorGUID(const std::string& id_or_guid) const;

  // Gets the IDs of all extensions in the set.
  ExtensionIdSet GetIDs() const;

  // Returns true if |info| should get extension api bindings and be permitted
  // to make api calls. Note that this is independent of what extension
  // permissions the given extension has been granted.
  bool ExtensionBindingsAllowed(const GURL& url) const;

  // Decodes extension ID encoded in URL. Returns the extension ID corresponding
  // to the given extension resource URL. This ignores hosted apps' web extent.
  //
  // Returns ExtensionId() if not an extension URL.
  static ExtensionId GetExtensionIdByURL(const GURL& url);

 private:
  ExtensionMap extensions_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSION_SET_H_
