// Builds valid digital identity request for navigator.identity.get() API.
export function buildValidNavigatorIdentityRequest() {
  return {
      digital: {
        providers: [{
          protocol: "protocol",
          selector: {
            format: ['mdoc'],
            doctype: 'org.iso.18013.5.1.mDL',
            fields: [
              'org.iso.18013.5.1.family_name',
              'org.iso.18013.5.1.portrait',
            ]
          },
          params: {
            nonce: '1234',
            readerPublicKey: 'test_reader_public_key',
            extraParamAsNeededByDigitalCredentials: true,
          },
        }],
      },
  };
}
