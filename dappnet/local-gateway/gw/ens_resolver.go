package lib

import (
	"log"
	"os"
	// "math/big"
	"encoding/json"

	"github.com/umbracle/ethgo"
	"github.com/umbracle/ethgo/abi"
	"github.com/umbracle/ethgo/contract"
	"github.com/umbracle/ethgo/jsonrpc"
	// "github.com/ethereum/go-ethereum/ethclient"
	ens "github.com/wealdtech/go-ens/v3"
)

var ensLogger = log.New(os.Stdout, "[ENS] ", log.Ldate|log.Ltime)

func handleErr(err error) {
	if err != nil {
		panic(err)
	}
}

var ENSRegistryAddress = ethgo.HexToAddress("0x00000000000C2E074eC69A0dFb2997BA6C7d2e1e")
var ENSRegistryFunctions = []string{
	"function resolver(bytes32 node) external view returns (address)",
}

var ENSResolverFunctions = []string{
	"function contenthash(bytes32 node) external view returns (bytes memory)",
}


func resolveENS(name string) []byte {
	ensLogger.Printf("resolveENS %s\n", name)

	namehash, err := ens.NameHash(name)
	handleErr(err)

	client, err := jsonrpc.NewClient("https://eth-mainnet.g.alchemy.com/v2/HXJKahkFFDDvPADHRfw5R")
	handleErr(err)

	// Step 1: Get the resolver address from the ENS registry
	// 
	abiContract1, err := abi.NewABIFromList(ENSRegistryFunctions)
	handleErr(err)

	ENSRegistry := contract.NewContract(ENSRegistryAddress, abiContract1, contract.WithJsonRPC(client.Eth()))
	handleErr(err)

	res, err := ENSRegistry.Call("resolver", ethgo.Latest, namehash)
	handleErr(err)

	jsonData, err := json.Marshal(res)
	handleErr(err)
	ensLogger.Println(string(jsonData))

	resolverAddress := res["0"].(ethgo.Address)
	ensLogger.Printf("Resolver: %s\n", resolverAddress)

	// Step 2: Get the contenthash from the resolver
	// 

	abiContract2, err := abi.NewABIFromList(ENSResolverFunctions)
	handleErr(err)

	ENSResolver := contract.NewContract(
		resolverAddress,
		abiContract2, 
		contract.WithJsonRPC(client.Eth()),
	)
	handleErr(err)

	res, err = ENSResolver.Call("contenthash", ethgo.Latest, namehash)
	handleErr(err)

	jsonData, err = json.Marshal(res)
	handleErr(err)

	ensLogger.Println(string(jsonData))

	// NOTE: I don't know why the key here is `memory`. Maybe it denotes `string memory`?
	contenthash := res["memory"].([]uint8)

	ensLogger.Printf("Contenthash: %#x\n", contenthash)

	str, err := ens.ContenthashToString(contenthash)
	handleErr(err)

	ensLogger.Printf("Contenthash: %s\n", str)

	return contenthash
}


type ENSResolver struct {
	cache map[string]string
}

func NewENSResolver() *ENSResolver {
	return &ENSResolver{
		cache: make(map[string]string),
	}
}

func (r *ENSResolver) Resolve(name string) string {
	if v, ok := r.cache[name]; ok {
		return v
	}

	contenthash := resolveENS(name)
	contenthash_str, err := ens.ContenthashToString(contenthash)
	handleErr(err)
	r.cache[name] = contenthash_str
	return contenthash_str
}