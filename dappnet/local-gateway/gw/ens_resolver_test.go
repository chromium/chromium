package lib

import (
	"testing"
)

func TestResolveENS(t *testing.T) {
    name := "kwenta.eth"
	t.Log("Testing ENS resolution for", name)
	resolveENS(name)
	// t.ExpectEqual("0x1f9840a85d5af5bf1d1762f925bdaddc4201f984", resolveENS(name))
}
