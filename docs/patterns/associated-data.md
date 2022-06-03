# The Associated Data Pattern

The associated data pattern allows a class C that is used by multiple other
classes *outside* its own module (which don't control or know the lifetime of C)
to store data that is logically "per instance of C". This document will call
those other classes the "consumer classes" of C.

Imagine that you have a class Pokemon, which is used by two other classes,
PokemonGroomer and PokemonNurse. A PokemonGroomer wishes to store the last time
a given Pokemon got a bath, and a PokemonNurse wishes to store the last time a
given Pokemon got a checkup.

You might implement that this way:

    class Pokemon {
      ...
      Time last_bath_;
      Time last_checkup_;
      ...
    };

    bool PokemonNurse::NeedsCheckup(Pokemon* p) {
      return Time::Now() - p->last_checkup_ > kCheckupInterval;
    }

but then Pokemon ends up containing (and being responsible for enforcing
invariants on) data that is only for the use of other objects, which might live
in entirely separate modules.

You might instead do this:

    class PokemonNurse {
      ...
      map<Pokemon*, Time> last_checkup_;
      ...
    };

    bool PokemonNurse::NeedsCheckup(Pokemon* p) {
      if (!last_checkup_.contains(p))
        last_checkup_[p] = 0;
      return Time::Now() - last_checkup_[p] > kCheckupInterval;
    }

but another problem appears: PokemonNurse has to know when Pokemon are released
into the wild so it can clean up the map, which adds extra bookkeeping.

The associated data pattern would look like this:

    class Pokemon {
      ...
      struct Data {
        virtual ~Data();
      };
      map<Key, unique_ptr<Data>> user_data_;
      ...
    };

    class PokemonNurse::PokemonData : public Pokemon::Data {
      Time last_checkup_;
    };

    bool PokemonNurse::NeedsCheckup(Pokemon* p) {
      if (!p->user_data_[PokemonNurse::kDataKey])
        p->user_data_[PokemonNurse::kDataKey] = make_unique<PokemonData>();
      return Time::Now() - p->user_data_[PokemonNurse::kDataKey].last_checkup_
          > kCheckupInterval;
    }

This way, Pokemon manages the *lifetime* of the per-Pokemon data, but
PokemonNurse manages the *invariants* of the data, and only PokemonNurse is
aware of the per-Pokemon data belonging to PokemonNurse.

## Use this pattern when:

*   You have a central class C, and many places in your code want to store some
    data for each instance of C.
*   C can't directly contain your data for layering or code structure reasons.
*   The data being stored is ephemeral and your consumer classes don't care
    about its lifetime.

## Don't use this pattern when:

*   The data being stored must expose any behavior other than a destructor to
    the central class; in that case, you need a richer pattern than this one.
*   The data being stored has a complex lifetime.
*   Deleting the data being stored has side effects.
*   There might be a very large amount (thousands) of associated data entries
    for a particular client class; in that case, a domain-specific data model
    will provide better efficiency.
*   The central class C and the consumer classes are close enough that it would
    make sense for C to store the data directly.

## Alternatives / See also:

*   Storing data for consumer classes directly on the object of class C.
*   Having the consumer store a map between objects of class C and the
    consumer's C-specific data.

## How to use this pattern in Chromium:

The two most commonly-used instances of this pattern in Chromium are
[SupportsUserData] (especially [WebContentsUserData] and [WebStateUserData]) and
[KeyedService] (usually via [BrowserContextKeyedServiceFactory] and
[BrowserStateKeyedServiceFactory]).

### SupportsUserData

SupportsUserData is a mixin-type class that allows consumer classes to stash
data on the class with the mixin. It exposes a very small interface
`SupportsUserData::Data` which stored data items implement (in fact, any class
with a virtual destructor already implements it), and adds a handful of new
methods to the class with the mixin:

    Data* GetUserData(const void* key);
    void SetUserData(const void* key, std::unique_ptr<Data> data);
    void RemoveUserData(const void* key);

For example, in //net, URLRequest inherits from SupportsUserData, so if you were
running a RequestNicenessService that wanted to annotate URLRequests with a
niceness value at one point and use that value later, you might do:

    class RequestNicenessService {
      static char kRequestDataKey;

      void SetRequestNiceness(net::URLRequest* request, int niceness);
      int GetRequestNiceness(net::URLRequest* request);
    };

    struct NicenessData : public SupportsUserData::Data {
      int niceness;
    };

    void RequestNicenessService::SetRequestNiceness(...) {
      request->SetUserData(&kRequestDataKey,
                           make_unique<NicenessData>(niceness));
    }

    int RequestNicenessService::GetRequestNiceness(...) {
      SupportsUserData::Data* data = request->GetUserData(&kRequestDataKey);
      if (data)
        return static_cast<NicenessData*>(data)->niceness;
      return -1;
    }

Or, if there might be multiple RequestNicenessService instances, you could use
the address of the RequestNicenessService instance itself as the key for the
UserData on the URLRequest. That's actually a more common pattern, but for a
singleton instance using the address of a static is also fine.

### WebContentsUserData & TabHelper

For the specific very common case of wanting to attach a SupportsUserData::Data
to a WebContents, the helper class [WebContentsUserData] exists. This templated
class handles casting to and from your concrete type for you. It is commonly
used with the related [TabHelper] pattern.

### KeyedService & BrowserContextKeyedServiceFactory

KeyedService is an abstract interface that implements this pattern, but with an
additional notion of dependencies between different pieces of associated data.
It is usually concretely used via BrowserContextKeyedServiceFactory, by
subclassing BrowserContextKeyedServiceFactory and overriding the
`BuildServiceInstanceFor` method. Since a Profile is a BrowserContext, this
provides a way to store data associated with a Profile. For example, if you
were creating a UserNicenessController, you might do:

    class UserNicenessData : public KeyedService {
     public:
      static UserNicenessData* FromProfile(Profile* profile);
      int niceness;

      // Maybe also:
      //   void Shutdown() override;
      // if you need to participate in KeyedService's two-phase destruction
      // protocol.
    };

    class UserNicenessController :
          public BrowserContextKeyedServiceFactory {
      ...
    };

    // static
    UserNicenessData*
    UserNicenessData::FromProfile(Profile* profile) {
      return static_cast<UserNicenessData*>(
          UserNicenessController::GetInstance()->
              GetServiceForBrowserContext(profile, true));
    }

    // in code somewhere:
    UserNicenessData::FromProfile(profile)->niceness++;

Any code that needs the UserNicenessData for a given Profile can call
`UserNicenessData::FromProfile`, and KeyedService and
BrowserContextKeyedServiceFactory will handle creation and lifetime as needed,
as well as handling any dependencies UserNicenessData might declare on other
services.

Note that the naming of KeyedService **implies** that it is intended for
services, as does the presence of a dependency mechanism, but the same approach
can be used to store plain data, as in this example.

Another note: Since `Profile` already SupportsUserData, if you simply want to
store data on the profile, it is easier to do it that way rather than use
KeyedService.

A third note: on iOS, use [BrowserStateKeyedServiceFactory] instead, which
attaches a KeyedService to a [BrowserState].

[BrowserContextKeyedServiceFactory]: https://chromium.googlesource.com/chromium/src/+/HEAD/components/keyed_service/content/browser_context_keyed_service_factory.h
[BrowserStateKeyedServiceFactory]: https://chromium.googlesource.com/chromium/src/+/HEAD/components/keyed_service/ios/browser_state_keyed_service_factory.h
[BrowserState]: https://chromium.googlesource.com/chromium/src/+/HEAD/ios/web/public/browser_state.h
[KeyedService]: https://chromium.googlesource.com/chromium/src/+/HEAD/components/keyed_service/core/keyed_service.h
[SupportsUserData]: https://chromium.googlesource.com/chromium/src/+/HEAD/base/supports_user_data.h
[TabHelper]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/tab_helpers.md
[WebContentsUserData]: https://chromium.googlesource.com/chromium/src/+/HEAD/content/public/browser/web_contents_user_data.h
[WebStateUserData]: https://chromium.googlesource.com/chromium/src/+/HEAD/ios/web/public/web_state_user_data.h
