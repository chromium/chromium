
- [x] Build the ENS + IPFS resolver.
- [x] Run a local IPFS node.
- [x] Disable search omnibox for .dapp and .eth domains.
- [x] Change the browser icon.
- [ ] Start local gateway on a random port. That way we can always launch.
- [ ] Bundle the golang local-gateway into chrome.
- [ ] Build the local gateway for macOS, Windows, Linux. Bundle it in.
- [ ] Figure out a way for MIME types to pass through with 100% effectiveness.
    - [ ] see ens.eth
- [ ] Figure out simple way for people to deploy HTML content to the wider world wide web from their computer.
- [ ] Write my own domain manager:
    - [ ] Blockchain name -> IP address. (scihub)
    - [ ] Blockchain name -> IPFS content. (dapps)
- [ ] Incorporate a static content viewer.
    - [ ] ipfs:// URI's
    - [ ] dapp:// URI's
- [ ] Write a UI for publishing content.
- [ ] Incorporate a local Ethereum node.
- [ ] Update info in `chrome/installer/linux/common/chromium-browser.info`
- [ ] Add a settings page to manage Ethereum nodes.
- [ ] Build a simple way to get government data in a way which is simple and viewable and accessible and purely based on code. All signed and authenticated.


http://vitalik.eth/


Built in:
- Observable P2P
    Shows number of peers, aggregate download rate
    Webtorrent video streaming
    Built-in file sharing, music client, video client, text client
    Save and pin stuff to your websites. This is a scientific device.
    Allow AI agents to crawl and use it.
    https://x.com/SCBuergel
- Domain Manager
    Buy domains using USDC and credit card etc.
    Deploy content to your own website
- Deployer
    Deploy media to the Dappnet
    Pay $$
    Content is hosted on superfast P2P nodes (IPFS, BitTorrent)
- Dapp Store
    Explore different decentralised apps
    Deploy your own
    Check the locality of the apps.
    They upgrade in the background
    Get older versions of dapps
- Settings page
    Ethereum    
        RPC nodes string[]
    IPFS
        port
        clear cache
    Dapps


Maybe we can have local app bundles?
Remove all external services. Just HTML + JS + CSS.
Fuck it. Just decentralised domains for now.

You need something that looks like:
- apps are bundled into .zip files of sorts.
- these are downloaded once only? or you can download on demand using IPFS.
- there needs to be a dapp store which lists things in terms of their network connections
- ie. 
    Uniswap
    (X) Local (not local)
    
    Relies on servers:
    - uniswap.org
    - etc.
- And apps which are local:
    Curve
    1inch
    Tornado Cash
        Relies on:
        - Relayer

Apps need to statically define which domains they are connecting to. 

Apps can be P2P and updated in the background.

There needs to be:
- static bundle URL scheme
- a hosting platform for IPFS nodes and stuff. where you can buy using tokens. and it is anonymous.
    where you can buy proxies to deploy media.

What about customers?
- Uniswap

