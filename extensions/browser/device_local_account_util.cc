// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/device_local_account_util.h"

#include <algorithm>

namespace extensions {

namespace {

// Apps/extensions explicitly whitelisted for use in public sessions.
const char* const kPublicSessionWhitelist[] = {
    // Public sessions in general:
    "cbkkbcmdlboombapidmoeolnmdacpkch",  // Chrome RDP
    "djflhoibgkdhkhhcedjiklpkjnoahfmg",  // User Agent Switcher
    "iabmpiboiopbgfabjmgeedhcmjenhbla",  // VNC Viewer
    "haiffjcadagjlijoggckpgfnoeiflnem",  // Citrix Receiver
    "lfnfbcjdepjffcaiagkdmlmiipelnfbb",  // Citrix Receiver (branded)
    "mfaihdlpglflfgpfjcifdjdjcckigekc",  // ARC Runtime
    "ngjnkanfphagcaokhjecbgkboelgfcnf",  // Print button
    "cjanmonomjogheabiocdamfpknlpdehm",  // HP printer driver
    "ioofdkhojeeimmagbjbknkejkgbphdfl",  // RICOH Print for Chrome
    "pmnllmkmjilbojkpgplbdmckghmaocjh",  // Scan app by François Beaufort
    "haeblkpifdemlfnkogkipmghfcbonief",  // Charismathics Smart Card Middleware
    "mpnkhdpphjiihmlmkcamhpogecnnfffa",  // Service NSW Kiosk Utility
    "npilppbicblkkgjfnbmibmhhgjhobpll",  // QwickACCESS
    // TODO(isandrk): Only on the whitelist for the purpose of getting the soft MGS warning.  Remove
    // once dynamic MGS warnings are implemented.
    "ppkfnjlimknmjoaemnpidmdlfchhehel",  // VMware Horizon Client for Chrome

    // Libraries:
    "aclofikceldphonlfmghmimkodjdmhck",  // Ancoris login component
    "eilbnahdgoddoedakcmfkcgfoegeloil",  // Ancoris proxy component
    "ceehlgckkmkaoggdnjhibffkphfnphmg",  // Libdata login
    "fnhgfoccpcjdnjcobejogdnlnidceemb",  // OverDrive

    // Education:
    "cmeclblmdmffdgpdlifgepjddoplmmal",  //  Imagine Learning

    // Retail mode:
    "bjfeaefhaooblkndnoabbkkkenknkemb",  // 500 px demo
    "ehcabepphndocfmgbdkbjibfodelmpbb",  // Angry Birds demo
    "kgimkbnclbekdkabkpjhpakhhalfanda",  // Bejeweled demo
    "joodangkbfjnajiiifokapkpmhfnpleo",  // Calculator
    "fpgfohogebplgnamlafljlcidjedbdeb",  // Calendar demo
    "cdjikkcakjcdjemakobkmijmikhkegcj",  // Chrome Remote Desktop demo
    "jkoildpomkimndcphjpffmephmcmkfhn",  // Chromebook Demo App
    "lbhdhapagjhalobandnbdnmblnmocojh",  // Crackle demo
    "ielkookhdphmgbipcfmafkaiagademfp",  // Custom bookmarks
    "kogjlbfgggambihdjcpijgcbmenblimd",  // Custom bookmarks
    "ogbkmlkceflgpilgbmbcfbifckpkfacf",  // Custom bookmarks
    "pbbbjjecobhljkkcenlakfnkmkfkfamd",  // Custom bookmarks
    "jkbfjmnjcdmhlfpephomoiipbhcoiffb",  // Custom bookmarks
    "dgmblbpgafgcgpkoiilhjifindhinmai",  // Custom bookmarks
    "iggnealjakkgfofealilhkkclnbnfnmo",  // Custom bookmarks
    "lplkobnahgbopmpkdapaihnnojkphahc",  // Custom bookmarks
    "lejnflfhjpcannpaghnahbedlabpmhoh",  // Custom bookmarks
    "dhjmfhojkfjmfbnbnpichdmcdghdpccg",  // Cut the Rope demo
    "ebkhfdfghngbimnpgelagnfacdafhaba",  // Deezer demo
    "npnjdccdffhdndcbeappiamcehbhjibf",  // Docs.app demo
    "ekgadegabdkcbkodfbgidncffijbghhl",  // Duolingo demo
    "iddohohhpmajlkbejjjcfednjnhlnenk",  // Evernote demo
    "bjdhhokmhgelphffoafoejjmlfblpdha",  // Gmail demo
    "nldmakcnfaflagmohifhcihkfgcbmhph",  // Gmail offline demo
    "mdhnphfgagkpdhndljccoackjjhghlif",  // Google Drive demo
    "dondgdlndnpianbklfnehgdhkickdjck",  // Google Keep demo
    "amfoiggnkefambnaaphodjdmdooiinna",  // Google Play Movie and TV demo
    "fgjnkhlabjcaajddbaenilcmpcidahll",  // Google+ demo
    "ifpkhncdnjfipfjlhfidljjffdgklanh",  // Google+ Photos demo
    "cgmlfbhkckbedohgdepgbkflommbfkep",  // Hangouts.app demo
    "ndlgnmfmgpdecjgehbcejboifbbmlkhp",  // Hash demo
    "edhhaiphkklkcfcbnlbpbiepchnkgkpn",  // Helper.extension demo
    "jckncghadoodfbbbmbpldacojkooophh",  // Journal demo
    "diehajhcjifpahdplfdkhiboknagmfii",  // Kindle demo
    "idneggepppginmaklfbaniklagjghpio",  // Kingsroad demo
    "nhpmmldpbfjofkipjaieeomhnmcgihfm",  // Menu.app demo
    "kcjbmmhccecjokfmckhddpmghepcnidb",  // Mint demo
    "onbhgdmifjebcabplolilidlpgeknifi",  // Music.app demo
    "kkkbcoabfhgekpnddfkaphobhinociem",  // Netflix demo
    "adlphlfdhhjenpgimjochcpelbijkich",  // New York Times demo
    "cgefhjmlaifaamhhoojmpcnihlbddeki",  // Pandora demo
    "kpjjigggmcjinapdeipapdcnmnjealll",  // Pixlr demo
    "ifnadhpngkodeccijnalokiabanejfgm",  // Pixsta demo
    "klcojgagjmpgmffcildkgbfmfffncpcd",  // Plex demo
    "nnikmgjhdlphciaonjmoppfckbpoinnb",  // Pocket demo
    "khldngaiohpnnoikfmnmfnebecgeobep",  // Polarr Photo demo
    "aleodiobpjillgfjdkblghiiaegggmcm",  // Quickoffice demo
    "nifkmgcdokhkjghdlgflonppnefddien",  // Sheets demo
    "hdmobeajeoanbanmdlabnbnlopepchip",  // Slides demo
    "ikmidginfdcbojdbmejkeakncgdbmonc",  // Soundtrap demo
    "dgohlccohkojjgkkfholmobjjoledflp",  // Spotify demo
    "dhmdaeekeihmajjnmichlhiffffdbpde",  // Store.app demo
    "onklhlmbpfnmgmelakhgehkfdmkpmekd",  // Todoist demo
    "jeabmjjifhfcejonjjhccaeigpnnjaak",  // TweetDeck demo
    "gnckahkflocidcgjbeheneogeflpjien",  // Vine demo
    "pdckcbpciaaicoomipamcabpdadhofgh",  // Weatherbug demo
    "biliocemfcghhioihldfdmkkhnofcgmb",  // Webcam Toy demo
    "bhfoghflalnnjfcfkaelngenjgjjhapk",  // Wevideo demo
    "pjckdjlmdcofkkkocnmhcbehkiapalho",  // Wunderlist demo
    "pbdihpaifchmclcmkfdgffnnpfbobefh",  // YouTube demo

    // New demo mode:
    "lpmakjfjcconjeehbidjclhdlpjmfjjj",  // Highlights app
    "iggildboghmjpbjcpmobahnkmoefkike",  // Highlights app (eve)
    "elhbopodaklenjkeihkdhhfaghalllba",  // Highlights app (nocturne)
    "gjeelkjnolfmhphfhhjokaijbicopfln",  // Highlights app (other)
    "mnoijifedipmbjaoekhadjcijipaijjc",  // Screensaver
    "gdobaoeekhiklaljmhladjfdfkigampc",  // Screensaver (eve)
    "lminefdanffajachfahfpmphfkhahcnj",  // Screensaver (nocturne)
    "bnabjkecnachpogjlfilfcnlpcmacglh",  // Screensaver (other)

    // Testing extensions:
    "ongnjlefhnoajpbodoldndkbkdgfomlp",  // Show Managed Storage
    "ilnpadgckeacioehlommkaafedibdeob",  // Enterprise DeviceAttributes
    "oflckobdemeldmjddmlbaiaookhhcngo",  // Citrix Receiver QA version
    "ljacajndfccfgnfohlgkdphmbnpkjflk",  // Chrome Remote Desktop (Dev Build)
    "behllobkkfkfnphdnhnkndlbkcpglgmj",  // Autotest

    // Google Apps:
    "mclkkofklkfljcocdinagocijmpgbhab",  // Google input tools
    "gbkeegbaiigmenfmjfclcdgdpimamgkj",  // Office Editing Docs/Sheets/Slides
    "aapbdbdomjkkjkaonfhkkikfgjllcleb",  // Google Translate
    "mgijmajocgfcbeboacabfgobmjgjcoja",  // Google Dictionary
    "mfhehppjhmmnlfbbopchdfldgimhfhfk",  // Google Classroom
    "mkaakpdehdafacodkgkpghoibnmamcme",  // Google Drawings
    "pnhechapfaindjhompbnflcldabbghjo",  // Secure Shell
    "fcgckldmmjdbpdejkclmfnnnehhocbfp",  // Google Finance
    "jhknlonaankphkkbnmjdlpehkinifeeg",  // Google Forms
    "jndclpdbaamdhonoechobihbbiimdgai",  // Chromebook Recovery Utility
    "aohghmighlieiainnegkcijnfilokake",  // Google Docs
    "eemlkeanncmjljgehlbplemhmdmalhdc",  // Chrome Connectivity Diagnostics
    "eoieeedlomnegifmaghhjnghhmcldobl",  // Google Apps Script
    "ndjpildffkeodjdaeebdhnncfhopkajk",  // Network File Share for Chrome OS
    "pfoeakahkgllhkommkfeehmkfcloagkl",  // Fusion Tables
    "aapocclcgogkmnckokdopfmhonfmgoek",  // Google Slides
    "khpfeaanjngmcnplbdlpegiifgpfgdco",  // Smart Card Connector
    "hmjkmjkepdijhoojdojkdfohbdgmmhki",  // Google Keep - notes and lists
    "felcaaldnbdncclmgdcncolpebgiejap",  // Google Sheets
    "gbchcmhmhahfdphkhkmpfmihenigjmpp",  // Chrome Remote Desktop
    "khkjfddibboofomnlkndfedpoccieiee",  // Study Kit
    "becloognjehhioodmnimnehjcibkloed",  // Coding with Chrome
    "hfhhnacclhffhdffklopdkcgdhifgngh",  // Camera
    "adokjfanaflbkibffcbhihgihpgijcei",  // Share to Classroom
    "heildphpnddilhkemkielfhnkaagiabh",  // Legacy Browser Support
    "lpcaedmchfhocbbapmcbpinfpgnhiddi",  // Google Keep Chrome Extension
    "ldipcbpaocekfooobnbcddclnhejkcpn",  // Google Scholar Button
    "nnckehldicaciogcbchegobnafnjkcne",  // Google Tone
    "pfmgfdlgomnbgkofeojodiodmgpgmkac",  // Data Saver
    "djcfdncoelnlbldjfhinnjlhdjlikmph",  // High Contrast
    "ipkjmjaledkapilfdigkgfmpekpfnkih",  // Color Enhancer
    "kcnhkahnjcbndmmehfkdnkjomaanaooo",  // Google Voice
    "nlbjncdgjeocebhnmkbbbdekmmmcbfjd",  // RSS Subscription Extension
    "aoggjnmghgmcllfenalipjhmooomfdce",  // SAML SSO for Chrome Apps
    "fhndealchbngfhdoncgcokameljahhog",  // Certificate Enrollment for Chrome OS
    "npeicpdbkakmehahjeeohfdhnlpdklia",  // WebRTC Network Limiter
    "hdkoikmfpncabbdniojdddokkomafcci",  // SSRS Reporting Fix for Chrome
};

}  // namespace

bool IsWhitelistedForPublicSession(const std::string& extension_id) {
  return std::find(std::begin(kPublicSessionWhitelist),
                   std::end(kPublicSessionWhitelist),
                   extension_id) != std::end(kPublicSessionWhitelist);
}

}  // namespace extensions
