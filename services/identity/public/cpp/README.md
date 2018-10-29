IdentityManager is the next-generation C++ API for interacting with Google
identity. It is currently backed by //components/signin (see IMPLEMENTATION
NOTES below); in the long-term it will serve as the primary client-side
interface to the Identity Service, encapsulating a connection to a remote
implementation of identity::mojom::IdentityManager. It provides conveniences
over the bare Identity Service Mojo interfaces such as:

- Synchronous access to the information of the primary account (via caching)

Documentation on the mapping between usage of legacy signin
classes (notably SigninManager(Base) and ProfileOAuth2TokenService) and usage of
IdentityManager is available here:

https://docs.google.com/document/d/14f3qqkDM9IE4Ff_l6wuXvCMeHfSC9TxKezXTCyeaPUY/edit#

A quick inline cheat sheet for developers migrating from usage of //components/
signin and //google_apis/gaia:

- "Primary account" in IdentityManager refers to what is called the
  "authenticated account" in SigninManager, i.e., the account that has been
  blessed for sync by the user.
- PrimaryAccountTokenFetcher is the primary client-side interface for obtaining
  access tokens for the primary account. In particular, it can take care of 
  waiting until the primary account is available.
- AccessTokenFetcher is the client-side interface for obtaining access tokens
  for arbitrary accounts.
- IdentityTestEnvironment is the preferred test infrastructure for unittests
  of production code that interacts with IdentityManager. It is suitable for
  use in cases where neither the production code nor the unittest is interacting
  with Profile (e.g., //components-level unittests).
- identity_test_utils.h provides lower-level test facilities for interacting
  explicitly with IdentityManager and its dependencies (SigninManager,
  ProfileOAuth2TokenService). These facilities are the way to interact with
  IdentityManager in unittest contexts where the production code and/or the
  unittest are interacting with Profile (in particular, where the
  IdentityManager instance with which the test is interacting must be
  IdentityManagerFactory::GetForProfile(profile)).

IMPLEMENTATION NOTES

The Identity Service client library is being developed in parallel with the
implementation and interfaces of the Identity Service itself. The motivation is
to allow clients to be converted to use this client library in a parallel and
pipelined fashion with building out the Identity Service as the backing
implementation of the library.

In the near term, this library is backed directly by //components/signin
classes. We are striving to make the interactions of this library with those
classes as similar as possible to its eventual interaction with the Identity
Service. In places where those interactions vary significantly from the
envisioned eventual interaction with the Identity Service, we have placed TODOs.
